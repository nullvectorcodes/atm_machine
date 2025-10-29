/* atm_system.c
   ATM Withdrawal System (console)
   - Accounts stored in accounts.txt
   - ATM inventory stored in atm.txt
   - Transactions appended to transactions.txt
   Compile: gcc atm_system.c -o atm_system
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define ACC_FILE "accounts.txt"
#define ATM_FILE "atm.txt"
#define TXN_FILE "transactions.txt"

#define MAX_NAME_LEN 50
#define MAX_ACCOUNTS 1000
#define MAX_LINE 256
#define ADMIN_PIN 999999  // default admin PIN (change as needed)

typedef struct {
    int accountNumber;
    int pin;
    double balance;
    char name[MAX_NAME_LEN];
    int loginAttempts; // consecutive failed attempts
    int locked; // 0 = unlocked, 1 = locked
} Account;

typedef struct {
    int note2000;
    int note500;
    int note200;
    int note100;
} ATM;

typedef struct {
    int accountNumber;
    char type[32];
    double amount;
    double remainingBalance;
    char datetime[64];
} Transaction;

/* Global arrays (simple approach) */
Account *accounts = NULL;
int accountCount = 0;
ATM atm = {0,0,0,0};

/* Utility prototypes */
int loadAccounts(const char *filename);
int saveAccounts(const char *filename);
int loadATM(const char *filename);
int saveATM(const char *filename);
void recordTransaction(const Transaction *t);
void showTransactionHistory(int accNum);
int findAccountIndex(int accNum);
int safeScanInt(const char *prompt);
double safeScanDouble(const char *prompt);
void flushStdin(void);
void printLine(void);

/* Core prototypes */
int login(int *accIndex);
void showBalance(const Account *acc);
void withdrawCash(Account *acc, ATM *atm);
void calculateDenominations(int amount, ATM *atm, int *n2000, int *n500, int *n200, int *n100, int *possible);
void adminMenu();

/* --------------------- Implementation --------------------- */

void printLine(void) {
    printf("--------------------------------------------------\n");
}

/* flush leftover input */
void flushStdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

/* Safe integer input with prompt */
int safeScanInt(const char *prompt) {
    char line[MAX_LINE];
    long val;
    while (1) {
        if (prompt) printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) {
            printf("Input error. Try again.\n");
            continue;
        }
        /* strip newline */
        char *p = line;
        while (*p && (*p==' '|| *p=='\t')) p++;
        if (*p=='\n' || *p=='\0') {
            printf("Empty input. Try again.\n");
            continue;
        }
        char extra;
        if (sscanf(line, "%ld %c", &val, &extra) == 1) {
            return (int)val;
        } else {
            printf("Invalid integer. Try again.\n");
        }
    }
}

/* Safe double input with prompt (for amounts) */
double safeScanDouble(const char *prompt) {
    char line[MAX_LINE];
    double val;
    while (1) {
        if (prompt) printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) {
            printf("Input error. Try again.\n");
            continue;
        }
        char *p = line;
        while (*p && (*p==' '|| *p=='\t')) p++;
        if (*p=='\n' || *p=='\0') {
            printf("Empty input. Try again.\n");
            continue;
        }
        char extra;
        if (sscanf(line, "%lf %c", &val, &extra) == 1) {
            return val;
        } else {
            printf("Invalid number. Try again.\n");
        }
    }
}

/* Load accounts from text file
   File format (one account per line):
   accountNumber pin balance name loginAttempts locked
   Example:
   1001 1234 15000.50 John_Doe 0 0
*/
int loadAccounts(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        /* No file - start with zero accounts */
        accountCount = 0;
        accounts = NULL;
        return 0;
    }
    accounts = malloc(sizeof(Account) * MAX_ACCOUNTS);
    if (!accounts) {
        printf("Memory allocation failed while loading accounts.\n");
        fclose(fp);
        return -1;
    }
    accountCount = 0;
    char namebuf[MAX_NAME_LEN];
    while (!feof(fp) && accountCount < MAX_ACCOUNTS) {
        int accNum, pin, loginAttempts, locked;
        double balance;
        if (fscanf(fp, "%d %d %lf %49s %d %d",
                   &accNum, &pin, &balance, namebuf, &loginAttempts, &locked) == 6) {
            accounts[accountCount].accountNumber = accNum;
            accounts[accountCount].pin = pin;
            accounts[accountCount].balance = balance;
            strncpy(accounts[accountCount].name, namebuf, MAX_NAME_LEN-1);
            accounts[accountCount].name[MAX_NAME_LEN-1] = '\0';
            accounts[accountCount].loginAttempts = loginAttempts;
            accounts[accountCount].locked = locked;
            accountCount++;
        } else {
            /* skip weird line */
            char skip[MAX_LINE];
            if (!fgets(skip, sizeof(skip), fp)) break;
        }
    }
    fclose(fp);
    return 0;
}

/* Save accounts back to file */
int saveAccounts(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Unable to open accounts file for writing.\n");
        return -1;
    }
    for (int i = 0; i < accountCount; ++i) {
        fprintf(fp, "%d %d %.2f %s %d %d\n",
                accounts[i].accountNumber,
                accounts[i].pin,
                accounts[i].balance,
                accounts[i].name,
                accounts[i].loginAttempts,
                accounts[i].locked);
    }
    fclose(fp);
    return 0;
}

/* Load ATM inventory from file
   Format: note2000 note500 note200 note100
   Example: 10 20 30 40
*/
int loadATM(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        /* default inventory if file missing */
        atm.note2000 = 10;
        atm.note500  = 20;
        atm.note200  = 50;
        atm.note100  = 100;
        return 0;
    }
    if (fscanf(fp, "%d %d %d %d", &atm.note2000, &atm.note500, &atm.note200, &atm.note100) != 4) {
        /* fallback defaults on parse fail */
        atm.note2000 = 10;
        atm.note500  = 20;
        atm.note200  = 50;
        atm.note100  = 100;
    }
    fclose(fp);
    return 0;
}

/* Save ATM inventory to file */
int saveATM(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: Unable to open ATM file for writing.\n");
        return -1;
    }
    fprintf(fp, "%d %d %d %d\n", atm.note2000, atm.note500, atm.note200, atm.note100);
    fclose(fp);
    return 0;
}

/* Append a transaction to transactions file */
void recordTransaction(const Transaction *t) {
    FILE *fp = fopen(TXN_FILE, "a");
    if (!fp) {
        printf("Warning: unable to open transaction file. Transaction not logged.\n");
        return;
    }
    fprintf(fp, "%d;%s;%.2f;%.2f;%s\n",
            t->accountNumber, t->type, t->amount, t->remainingBalance, t->datetime);
    fclose(fp);
}

/* Show transaction history for a particular account */
void showTransactionHistory(int accNum) {
    FILE *fp = fopen(TXN_FILE, "r");
    if (!fp) {
        printf("No transaction history found.\n");
        return;
    }
    char line[MAX_LINE];
    int found = 0;
    printLine();
    printf("Transaction History for Account %d\n", accNum);
    printLine();
    while (fgets(line, sizeof(line), fp)) {
        int a;
        char type[32], datetime[64];
        double amt, bal;
        /* format: acc;type;amt;bal;datetime\n */
        if (sscanf(line, "%d;%31[^;];%lf;%lf;%63[^\n]", &a, type, &amt, &bal, datetime) == 5) {
            if (a == accNum) {
                printf("[%s] %s : ₹%.2f | Balance: ₹%.2f\n", datetime, type, amt, bal);
                found = 1;
            }
        }
    }
    if (!found) {
        printf("No transactions found for this account.\n");
    }
    printLine();
    fclose(fp);
}

/* Find account index by account number */
int findAccountIndex(int accNum) {
    for (int i = 0; i < accountCount; ++i) {
        if (accounts[i].accountNumber == accNum) return i;
    }
    return -1;
}

/* Login routine: returns 1 if success and sets accIndex, else 0 */
int login(int *accIndex) {
    printLine();
    printf("Enter Account Number: ");
    int accNum = safeScanInt("");
    int idx = findAccountIndex(accNum);
    if (idx == -1) {
        printf("Account not found.\n");
        return 0;
    }
    Account *acc = &accounts[idx];
    if (acc->locked) {
        printf("Account is locked due to multiple failed login attempts. Contact admin.\n");
        return 0;
    }
    int attemptsLeft = 3 - acc->loginAttempts;
    for (;;) {
        printf("Enter PIN: ");
        int pin = safeScanInt("");
        if (pin == acc->pin) {
            acc->loginAttempts = 0; // reset on success
            *accIndex = idx;
            printf("Login successful. Welcome, %s!\n", acc->name);
            return 1;
        } else {
            acc->loginAttempts++;
            attemptsLeft = 3 - acc->loginAttempts;
            if (attemptsLeft > 0) {
                printf("Incorrect PIN. Attempts remaining: %d\n", attemptsLeft);
            } else {
                acc->locked = 1;
                printf("Incorrect PIN. Account locked after 3 failed attempts.\n");
                saveAccounts(ACC_FILE); // persist locked state
                return 0;
            }
        }
    }
}

/* Show account balance */
void showBalance(const Account *acc) {
    printLine();
    printf("Account: %d | Name: %s\n", acc->accountNumber, acc->name);
    printf("Available Balance: ₹ %.2f\n", acc->balance);
    printLine();
    /* record inquiry as transaction */
    Transaction t;
    t.accountNumber = acc->accountNumber;
    strcpy(t.type, "Balance Inquiry");
    t.amount = 0.0;
    t.remainingBalance = acc->balance;
    time_t now = time(NULL);
    strftime(t.datetime, sizeof(t.datetime), "%Y-%m-%d %H:%M:%S", localtime(&now));
    recordTransaction(&t);
}

/* Calculate denominations greedily but respect ATM availability */
void calculateDenominations(int amount, ATM *atm, int *n2000, int *n500, int *n200, int *n100, int *possible) {
    int remaining = amount;
    *n2000 = *n500 = *n200 = *n100 = 0;
    /* start with 2000 */
    int use = remaining / 2000;
    if (use > atm->note2000) use = atm->note2000;
    *n2000 = use;
    remaining -= use * 2000;

    /* 500 */
    use = remaining / 500;
    if (use > atm->note500) use = atm->note500;
    *n500 = use;
    remaining -= use * 500;

    /* 200 */
    use = remaining / 200;
    if (use > atm->note200) use = atm->note200;
    *n200 = use;
    remaining -= use * 200;

    /* 100 */
    use = remaining / 100;
    if (use > atm->note100) use = atm->note100;
    *n100 = use;
    remaining -= use * 100;

    if (remaining == 0) {
        *possible = 1;
        return;
    }

    /* If greedy failed (because of insufficient medium notes), attempt limited backtracking:
       Try reducing some higher denomination usage to make change using smaller notes.
       This is a simple heuristic, not exhaustive search, but sufficient for typical note sets.
    */
    int backup2000 = *n2000, backup500 = *n500, backup200 = *n200, backup100 = *n100;

    /* Try reduce 2000 by 1 repeatedly and recalc remainder */
    for (int r2000 = backup2000; r2000 >= 0 && !(*possible); --r2000) {
        for (int r500 = backup500; r500 >= 0 && !(*possible); --r500) {
            for (int r200 = backup200; r200 >= 0 && !(*possible); --r200) {
                int rem = amount - (r2000*2000 + r500*500 + r200*200);
                if (rem < 0) continue;
                int need100 = rem / 100;
                if (rem % 100 != 0) continue;
                if (need100 <= atm->note100) {
                    /* solution found */
                    *n2000 = r2000;
                    *n500  = r500;
                    *n200  = r200;
                    *n100  = need100;
                    *possible = 1;
                    return;
                }
            }
        }
    }

    /* if still not possible */
    *n2000 = backup2000;
    *n500  = backup500;
    *n200  = backup200;
    *n100  = backup100;
    *possible = 0;
}

/* Withdraw cash */
void withdrawCash(Account *acc, ATM *atm) {
    printf("Enter amount to withdraw (multiples of 100): ");
    double damount = safeScanDouble("");
    if (damount <= 0.0) {
        printf("Invalid amount. Must be > 0.\n");
        return;
    }
    if (((int)damount) % 100 != 0) {
        printf("Amount must be a multiple of 100.\n");
        return;
    }
    int amount = (int)damount;
    if (amount > (int)(acc->balance + 0.0001)) {
        printf("Insufficient balance.\n");
        return;
    }
    /* compute ATM's total cash */
    long atmTotal = (long)atm->note2000*2000 + (long)atm->note500*500 + (long)atm->note200*200 + (long)atm->note100*100;
    if (amount > atmTotal) {
        printf("ATM does not have enough cash.\n");
        return;
    }
    int n2000, n500, n200, n100, possible;
    calculateDenominations(amount, atm, &n2000, &n500, &n200, &n100, &possible);
    if (!possible) {
        printf("ATM cannot dispense the requested amount with available denominations.\n");
        return;
    }
    /* Show breakdown and ask for confirmation */
    printLine();
    printf("Dispensing:\n");
    if (n2000) printf("₹2000 x %d\n", n2000);
    if (n500)  printf("₹500  x %d\n", n500);
    if (n200)  printf("₹200  x %d\n", n200);
    if (n100)  printf("₹100  x %d\n", n100);
    printLine();
    printf("Confirm withdrawal? (1=Yes, 0=No): ");
    int confirm = safeScanInt("");
    if (!confirm) {
        printf("Withdrawal cancelled.\n");
        return;
    }
    /* Deduct from account and ATM */
    acc->balance -= amount;
    atm->note2000 -= n2000;
    atm->note500  -= n500;
    atm->note200  -= n200;
    atm->note100  -= n100;

    /* Record transaction */
    Transaction t;
    t.accountNumber = acc->accountNumber;
    strcpy(t.type, "Withdrawal");
    t.amount = (double)amount;
    t.remainingBalance = acc->balance;
    time_t now = time(NULL);
    strftime(t.datetime, sizeof(t.datetime), "%Y-%m-%d %H:%M:%S", localtime(&now));
    recordTransaction(&t);

    /* persist changes */
    saveAccounts(ACC_FILE);
    saveATM(ATM_FILE);

    printf("Transaction successful. New balance: ₹ %.2f\n", acc->balance);
}

/* Simple admin menu to view/refill ATM and unlock accounts */
void adminMenu() {
    printf("Enter admin PIN: ");
    int pin = safeScanInt("");
    if (pin != ADMIN_PIN) {
        printf("Invalid admin PIN.\n");
        return;
    }
    int choice;
    do {
        printLine();
        printf("Admin Menu:\n1. View ATM inventory\n2. Refill ATM notes\n3. View all accounts\n4. Unlock account\n5. Exit admin\nEnter choice: ");
        choice = safeScanInt("");
        if (choice == 1) {
            printLine();
            printf("ATM Inventory:\n₹2000 x %d\n₹500  x %d\n₹200  x %d\n₹100  x %d\n",
                   atm.note2000, atm.note500, atm.note200, atm.note100);
            printLine();
        } else if (choice == 2) {
            printf("Enter additional ₹2000 notes to add: ");
            int a = safeScanInt("");
            printf("Enter additional ₹500 notes to add: ");
            int b = safeScanInt("");
            printf("Enter additional ₹200 notes to add: ");
            int c = safeScanInt("");
            printf("Enter additional ₹100 notes to add: ");
            int d = safeScanInt("");
            if (a < 0 || b < 0 || c < 0 || d < 0) {
                printf("Invalid (negative) input. Operation cancelled.\n");
            } else {
                atm.note2000 += a;
                atm.note500  += b;
                atm.note200  += c;
                atm.note100  += d;
                saveATM(ATM_FILE);
                printf("ATM refilled successfully.\n");
            }
        } else if (choice == 3) {
            printLine();
            printf("Accounts List:\n");
            for (int i = 0; i < accountCount; ++i) {
                printf("Acc: %d | Name: %s | Bal: ₹%.2f | Locked: %s\n",
                       accounts[i].accountNumber, accounts[i].name, accounts[i].balance,
                       accounts[i].locked ? "Yes" : "No");
            }
            printLine();
        } else if (choice == 4) {
            printf("Enter account number to unlock: ");
            int accn = safeScanInt("");
            int idx = findAccountIndex(accn);
            if (idx == -1) {
                printf("Account not found.\n");
            } else {
                accounts[idx].locked = 0;
                accounts[idx].loginAttempts = 0;
                saveAccounts(ACC_FILE);
                printf("Account %d unlocked.\n", accn);
            }
        } else if (choice == 5) {
            printf("Exiting admin menu.\n");
        } else {
            printf("Invalid choice.\n");
        }
    } while (choice != 5);
}

/* Main program */
int main(void) {
    /* load data from files */
    if (loadAccounts(ACC_FILE) != 0) {
        printf("Error loading accounts.\n");
        return 1;
    }
    loadATM(ATM_FILE);

    /* if no accounts exist, create a sample set (so user can test) */
    if (accountCount == 0) {
        printf("No accounts found. Creating sample accounts for testing.\n");
        accountCount = 3;
        accounts = realloc(accounts, sizeof(Account) * accountCount);
        accounts[0].accountNumber = 1001; accounts[0].pin = 1234; accounts[0].balance = 15000.0; strcpy(accounts[0].name, "Zaid"); accounts[0].loginAttempts = 0; accounts[0].locked=0;
        accounts[1].accountNumber = 1002; accounts[1].pin = 2345; accounts[1].balance = 5000.0;  strcpy(accounts[1].name, "Anita"); accounts[1].loginAttempts = 0; accounts[1].locked=0;
        accounts[2].accountNumber = 1003; accounts[2].pin = 3456; accounts[2].balance = 20000.0; strcpy(accounts[2].name, "Ravi");  accounts[2].loginAttempts = 0; accounts[2].locked=0;
        saveAccounts(ACC_FILE);
        saveATM(ATM_FILE);
    }

    printf("Welcome to the ATM Withdrawal System (Console)\n");
    int mainChoice;
    do {
        printLine();
        printf("Main Menu:\n1. User Login\n2. Admin Menu\n3. Exit\nEnter choice: ");
        mainChoice = safeScanInt("");
        if (mainChoice == 1) {
            int accIndex;
            if (login(&accIndex)) {
                Account *acc = &accounts[accIndex];
                int userChoice;
                do {
                    printLine();
                    printf("1. Balance Inquiry\n2. Cash Withdrawal\n3. Transaction History\n4. Logout\nEnter choice: ");
                    userChoice = safeScanInt("");
                    if (userChoice == 1) {
                        showBalance(acc);
                    } else if (userChoice == 2) {
                        withdrawCash(acc, &atm);
                    } else if (userChoice == 3) {
                        showTransactionHistory(acc->accountNumber);
                    } else if (userChoice == 4) {
                        printf("Logging out...\n");
                        saveAccounts(ACC_FILE); // persist any changes
                    } else {
                        printf("Invalid choice.\n");
                    }
                } while (userChoice != 4);
            }
        } else if (mainChoice == 2) {
            adminMenu();
        } else if (mainChoice == 3) {
            printf("Exiting system. Goodbye!\n");
            saveAccounts(ACC_FILE);
            saveATM(ATM_FILE);
        } else {
            printf("Invalid choice.\n");
        }
    } while (mainChoice != 3);

    /* cleanup */
    if (accounts) free(accounts);
    return 0;
}
