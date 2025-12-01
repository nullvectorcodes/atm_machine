#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   // for strcasecmp
#include <ctype.h>     // for tolower
#include <time.h>

#define INITIAL_VEHICLE_CAPACITY 8
#define INITIAL_VIOLATION_CAPACITY 4
#define MAX_STR 128
#define GRID_ROWS 5
#define GRID_COLS 5

typedef enum { VEH_2WHEELER, VEH_4WHEELER, VEH_COMMERCIAL } VehicleType;
typedef enum { PAYMENT_PENDING, PAYMENT_PARTIAL, PAYMENT_PAID } PaymentStatus;
typedef enum { FALSE = 0, TRUE = 1 } bool;

typedef struct {
    char street[MAX_STR];
    char city[MAX_STR];
    char state[MAX_STR];
    char pin[16];
} Address;

typedef struct {
    int day, month, year;
} Date;

typedef struct {
    int hour, minute;
} TimeOfDay;

typedef struct {
    char officer_id[32];
    char officer_name[MAX_STR];
    char station[MAX_STR];
} Officer;

struct Violation;

typedef struct {
    double amount_due;
    double amount_paid;
    PaymentStatus status;
    Date due_date;
} Payment;

typedef struct Violation {
    char v_id[32];
    Date date;
    TimeOfDay time;
    char location[MAX_STR];
    int zone_row, zone_col;
    char violation_type[MAX_STR];
    double base_fine;
    double calculated_fine;
    char evidence_ref[MAX_STR];
    Officer issued_by;
    Payment payment;
    bool court_notice;
} Violation;

typedef struct {
    char reg_no[32];
    char owner_name[MAX_STR];
    char contact[MAX_STR];
    VehicleType type;
    char license_no[64];
    Address addr;
    int violation_count;
    Violation **violations;
    int vio_capacity;
} Vehicle;

static double cumulative_revenue = 0.0;
Vehicle **vehicle_db = NULL;
int vehicle_count = 0;
int vehicle_capacity = 0;
int hotspot_grid[GRID_ROWS][GRID_COLS] = {0};

double base_fine_for_type(const char *type) {
    if (strcasecmp(type, "Speeding") == 0) return 1000.0;
    if (strcasecmp(type, "No helmet") == 0 || strcasecmp(type, "No seatbelt") == 0) return 500.0;
    if (strcasecmp(type, "Signal jump") == 0) return 1500.0;
    if (strcasecmp(type, "Wrong parking") == 0) return 200.0;
    if (strcasecmp(type, "Drunk driving") == 0) return 5000.0;
    if (strcasecmp(type, "Triple riding") == 0) return 1000.0;
    if (strcasecmp(type, "Mobile") == 0 || strcasecmp(type, "Using mobile") == 0) return 1000.0;
    if (strcasecmp(type, "No documents") == 0) return 2000.0;
    if (strcasecmp(type, "School over-speed") == 0) return 2000.0;
    return 0.0;
}

void ensure_vehicle_capacity() {
    if (vehicle_db == NULL) {
        vehicle_capacity = INITIAL_VEHICLE_CAPACITY;
        vehicle_db = (Vehicle **) calloc(vehicle_capacity, sizeof(Vehicle *));
        return;
    }
    if (vehicle_count >= vehicle_capacity) {
        vehicle_capacity *= 2;
        vehicle_db = (Vehicle **) realloc(vehicle_db, vehicle_capacity * sizeof(Vehicle *));
        for (int i = vehicle_count; i < vehicle_capacity; ++i) vehicle_db[i] = NULL;
    }
}

void ensure_vehicle_violations_capacity(Vehicle *v) {
    if (v->violations == NULL) {
        v->vio_capacity = INITIAL_VIOLATION_CAPACITY;
        v->violations = (Violation **) calloc(v->vio_capacity, sizeof(Violation *));
        return;
    }
    if (v->violation_count >= v->vio_capacity) {
        v->vio_capacity *= 2;
        v->violations = (Violation **) realloc(v->violations, v->vio_capacity * sizeof(Violation *));
        for (int i = v->violation_count; i < v->vio_capacity; ++i) v->violations[i] = NULL;
    }
}

void trimnl(char *s) {
    size_t n = strlen(s);
    if (n == 0) return;
    if (s[n - 1] == '\n') s[n - 1] = 0;
}

Vehicle *find_vehicle_by_reg(const char *reg) {
    for (int i = 0; i < vehicle_count; ++i) {
        if (vehicle_db[i] && strcasecmp(vehicle_db[i]->reg_no, reg) == 0) {
            return vehicle_db[i];
        }
    }
    return NULL;
}

VehicleType parse_vehicle_type(const char *s) {
    if (strcasecmp(s, "2") == 0 || strcasecmp(s, "2-wheeler") == 0) return VEH_2WHEELER;
    if (strcasecmp(s, "4") == 0 || strcasecmp(s, "4-wheeler") == 0) return VEH_4WHEELER;
    return VEH_COMMERCIAL;
}

const char *vehicle_type_str(VehicleType vt) {
    switch (vt) {
        case VEH_2WHEELER: return "2-wheeler";
        case VEH_4WHEELER: return "4-wheeler";
        case VEH_COMMERCIAL: return "Commercial";
    }
    return "Unknown";
}

void generate_violation_id(char *buffer, size_t bufsz) {
    static int seq = 0;
    seq++;
    time_t t = time(NULL);
    snprintf(buffer, bufsz, "VIO-%ld-%d", (long)t, seq);
}

void register_vehicle_interactive() {
    char buf[MAX_STR];
    ensure_vehicle_capacity();
    Vehicle *v = (Vehicle *) malloc(sizeof(Vehicle));
    v->violations = NULL;
    v->violation_count = 0;
    v->vio_capacity = 0;

    printf("Enter vehicle registration number: ");
    fgets(buf, MAX_STR, stdin); trimnl(buf); strncpy(v->reg_no, buf, sizeof(v->reg_no));

    if (find_vehicle_by_reg(v->reg_no)) {
        printf("Vehicle with reg no '%s' already exists.\n", v->reg_no);
        free(v);
        return;
    }

    printf("Owner name: ");
    fgets(buf, MAX_STR, stdin); trimnl(buf); strncpy(v->owner_name, buf, sizeof(v->owner_name));
    printf("Contact number: ");
    fgets(buf, MAX_STR, stdin); trimnl(buf); strncpy(v->contact, buf, sizeof(v->contact));
    printf("Vehicle type (2/4/c): ");
    fgets(buf, MAX_STR, stdin); trimnl(buf); v->type = parse_vehicle_type(buf);
    printf("License number: ");
    fgets(buf, MAX_STR, stdin); trimnl(buf); strncpy(v->license_no, buf, sizeof(v->license_no));

    printf("Address - street: "); fgets(buf, MAX_STR, stdin); trimnl(buf); strncpy(v->addr.street, buf, sizeof(v->addr.street));
    printf("City: "); fgets(buf, MAX_STR, stdin); trimnl(buf); strncpy(v->addr.city, buf, sizeof(v->addr.city));
    printf("State: "); fgets(buf, MAX_STR, stdin); trimnl(buf); strncpy(v->addr.state, buf, sizeof(v->addr.state));
    printf("PIN: "); fgets(buf, 16, stdin); trimnl(buf); strncpy(v->addr.pin, buf, sizeof(v->addr.pin));

    vehicle_db[vehicle_count++] = v;
    printf("Vehicle registered successfully!\n");
}

void seed_sample_vehicles() {
    if (vehicle_count > 0) return;
    ensure_vehicle_capacity();

    Vehicle *v1 = malloc(sizeof(Vehicle));
    strcpy(v1->reg_no, "KA01AB1234");
    strcpy(v1->owner_name, "Ravi Kumar");
    strcpy(v1->contact, "9876543210");
    v1->type = VEH_4WHEELER;
    strcpy(v1->license_no, "DL-04-2022-AAA");
    strcpy(v1->addr.street, "MG Road");
    strcpy(v1->addr.city, "Bengaluru");
    strcpy(v1->addr.state, "Karnataka");
    strcpy(v1->addr.pin, "560001");
    v1->violations = NULL; v1->violation_count = 0; v1->vio_capacity = 0;

    Vehicle *v2 = malloc(sizeof(Vehicle));
    strcpy(v2->reg_no, "MH12CD4321");
    strcpy(v2->owner_name, "Priya Sharma");
    strcpy(v2->contact, "9123456780");
    v2->type = VEH_2WHEELER;
    strcpy(v2->license_no, "MH-12-2019-BBB");
    strcpy(v2->addr.street, "FC Road");
    strcpy(v2->addr.city, "Pune");
    strcpy(v2->addr.state, "Maharashtra");
    strcpy(v2->addr.pin, "411001");
    v2->violations = NULL; v2->violation_count = 0; v2->vio_capacity = 0;

    vehicle_db[vehicle_count++] = v1;
    vehicle_db[vehicle_count++] = v2;
}

void display_vehicle_details(Vehicle *v) {
    printf("\n--- Vehicle Details ---\n");
    printf("Reg No: %s\nOwner: %s\nContact: %s\nType: %s\nLicense: %s\nAddress: %s, %s, %s - %s\nViolations: %d\n",
           v->reg_no, v->owner_name, v->contact, vehicle_type_str(v->type),
           v->license_no, v->addr.street, v->addr.city, v->addr.state, v->addr.pin, v->violation_count);
}

void list_vehicles() {
    printf("\n==== Vehicle List ====\n");
    for (int i = 0; i < vehicle_count; i++) {
        display_vehicle_details(vehicle_db[i]);
    }
}

int main() {
    seed_sample_vehicles();
    int choice;
    while (1) {
        printf("\n--- TRAFFIC VIOLATION MANAGEMENT ---\n");
        printf("1. Register new vehicle\n");
        printf("2. List all vehicles\n");
        printf("0. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        getchar(); // clear newline

        switch (choice) {
            case 1: register_vehicle_interactive(); break;
            case 2: list_vehicles(); break;
            case 0: printf("Exiting...\n"); exit(0);
            default: printf("Invalid choice.\n"); break;
        }
    }
    return 0;
}
