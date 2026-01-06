#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#ifdef _WIN32
#include<windows.h>
#endif // _WIN32

#define MEDLIST_PATH           "Data\\Letter_X\\med_name.txt"
#define INVENTORY_PATH         "Data\\inventory.dat"           // current stock (per batch: name + expiry)
#define INVENTORY_TMP_PATH     "Data\\inventory.tmp"           // temp for safe rewrite
#define LOG_PATH               "Data\\log.dat"                 // append-only mixed log (M/S)
#define CUSTOMER_REQUEST       "Data\\coustomer_request.txt"
#define ADMIN_FILE             "Data\\admin.dat"
#define SALT "Pharma@2025"                                     //for encryption

// Color definitions
#define RED                    "\033[1;31m"
#define GREEN                  "\033[1;32m"
#define YELLOW                 "\033[1;33m"
#define BLUE                   "\033[1;34m"
#define MAGENTA                "\033[1;35m"
#define CYAN                   "\033[1;36m"
#define BASE_COLOR             CYAN
#define RESET                  BASE_COLOR

// ---------------------------
//         Structures
// ---------------------------
typedef struct {
    int id;                     // batch id
    char name[50];
    char expire_date[11];       // YYYY-MM-DD
    int total_units;            // units remaining in this batch
    char entry_datetime[20];
    float unit_price;    // YYYY-MM-DD HH:MM:SS
} Medicine;

typedef struct {
    char datetime[20];          // YYYY-MM-DD HH:MM:SS
    char customerName[50];
    char phone[20];
    char medName[50];
    char expire_date[11];       // record which batch (expiry) this sale used; empty if mixed/FIFO
    int quantity;
} Sale;

typedef struct {
    char username[50];
    unsigned long pass_hash;
    char sec_question[100];
    unsigned long sec_answer_hash;
} Admin;

// ---------------------------
// ---------------------------

// ---------------------------
//         Helpers
// ---------------------------
void logo()
{

    printf(YELLOW);
    FILE* logo;

    logo=fopen("logo.txt", "r");
    char line[1000];
    while(fgets(line,sizeof(line),logo)){
        printf("%s",line);
    }

    fclose(logo);

    printf("\n\n\n\n\n");
    printf(RESET);
}
#
bool Isdigit(char input)
{
    if(input >= '0' && input <='9')
        return true;
}

unsigned long simple_hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) ^ c;
    return hash;
}

unsigned long salted_hash(const char *text) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s%s", SALT, text);
    return simple_hash(buffer);
}

void getCurrentDateTime(char *buffer) {
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", info);
}

int confirm_change(const char *label, const char *value) {
    printf("%s: %s\n", label, value);
    char choice;
    printf("Confirm change? (y/n): ");
    scanf(" %c", &choice);
    return (choice == 'y' || choice == 'Y');
}

static int log_append_medicine(const Medicine *m) {
    FILE *log = fopen(LOG_PATH, "ab");
    if (!log) { printf(RED "Error opening log file!" RESET "\n"); return 0; }
    unsigned char tag = 'M';
    size_t ok = fwrite(&tag, 1, 1, log) == 1
             && fwrite(m, sizeof(Medicine), 1, log) == 1;
    fclose(log);
    return (int)ok;
}

static int log_append_sale(const Sale *s) {
    FILE *log = fopen(LOG_PATH, "ab");
    if (!log) { printf(RED "Error opening log file!" RESET "\n"); return 0; }
    unsigned char tag = 'S';
    size_t ok = fwrite(&tag, 1, 1, log) == 1
             && fwrite(s, sizeof(Sale), 1, log) == 1;
    fclose(log);
    return (int)ok;
}

void parseDate(const char *dateStr, struct tm *tmStruct) {
    memset(tmStruct, 0, sizeof(struct tm));
    int y=0,m=0,d=0;
    sscanf(dateStr, "%d-%d-%d", &y, &m, &d);
    tmStruct->tm_year = y - 1900;
    tmStruct->tm_mon  = m - 1;
    tmStruct->tm_mday = d;
}

int cmp_expire_asc(const void *pa, const void *pb) {
    const Medicine *a = (const Medicine*)pa;
    const Medicine *b = (const Medicine*)pb;
    struct tm ta, tb; parseDate(a->expire_date, &ta); parseDate(b->expire_date, &tb);
    time_t ea = mktime(&ta), eb = mktime(&tb);
    if (ea < eb) return -1; if (ea > eb) return 1; return 0;
}


void clearScreen() {

     printf("\033[%1;1H");
     printf("\033[3J");
     printf("\033[J");
     printf("\033[%1;1H");
     fflush(stdout);
     logo();
     }

void enable_utf8_console(void) {
#ifdef _WIN32

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

#endif
}
// ---------------------------
// ---------------------------


// ---------------------------
// Admin file operations
// ---------------------------
int init_admin_file() {
    FILE *f = fopen(ADMIN_FILE, "rb");
    if (f) { fclose(f); return 0; }

    Admin admin;
    strcpy(admin.username, "admin");
    admin.pass_hash = salted_hash("admin");
    strcpy(admin.sec_question, "What is your pharmacy license number?");
    admin.sec_answer_hash = salted_hash("1234");

    f = fopen(ADMIN_FILE, "wb");
    fwrite(&admin, sizeof(Admin), 1, f);
    fclose(f);

    clearScreen();
    return 1;
}

int read_admin(Admin *admin) {
    FILE *f = fopen(ADMIN_FILE, "rb");
    if (!f) return 0;
    fread(admin, sizeof(Admin), 1, f);
    fclose(f);
    return 1;
}

int write_admin(Admin *admin) {
    FILE *f = fopen(ADMIN_FILE, "wb");
    if (!f) return 0;
    fwrite(admin, sizeof(Admin), 1, f);
    fclose(f);
    return 1;
}
// ---------------------------
// ---------------------------


// ---------------------------
// Admin change operations
// ---------------------------
void change_username(Admin *admin) {
    char buffer[50];
    while (1) {
        clearScreen();
        printf("=== Change Username ===\n\n");
        printf("Enter new username: ");
        scanf("%49s", buffer);
        if (confirm_change("New username", buffer)) {
            strcpy(admin->username, buffer);
            write_admin(admin);
            printf(GREEN "Username updated successfully." RESET "\n");
            usleep(1500000);
            break;
        }
        printf("Re-enter username.\n");
        usleep(1000000);
    }
}

void change_password(Admin *admin) {
    char buffer[50];
    while (1) {
        clearScreen();
        printf("=== Change Password ===\n\n");
        printf("Enter new password: ");
        scanf("%49s", buffer);
        if (confirm_change("New password", buffer)) {
            admin->pass_hash = salted_hash(buffer);
            write_admin(admin);
            printf(GREEN "Password updated successfully." RESET "\n");
            usleep(1500000);
            break;
        }
        printf("Re-enter password.\n");
        usleep(1000000);
    }
}

void change_security(Admin *admin) {
    char qbuf[100], abuf[100];
    getchar();
    while (1) {
        clearScreen();
        printf("=== Change Security Settings ===\n\n");
        printf("Enter new security question: ");
        fgets(qbuf, sizeof(qbuf), stdin);
        qbuf[strcspn(qbuf, "\n")] = 0;
        printf("Enter new answer: ");
        fgets(abuf, sizeof(abuf), stdin);
        abuf[strcspn(abuf, "\n")] = 0;

        printf("Security Question: %s\n", qbuf);
        printf("Security Answer: %s\n", abuf);
        char choice;
        printf("Confirm change? (y/n): ");
        scanf(" %c", &choice);

        if (choice == 'y' || choice == 'Y') {
            strcpy(admin->sec_question, qbuf);
            admin->sec_answer_hash = salted_hash(abuf);
            write_admin(admin);
            printf(GREEN "Security question and answer updated." RESET "\n");
            usleep(1500000);
            break;
        } else {
            printf("Re-enter security details.\n");
            usleep(1500000);
            getchar();
        }
    }
}
// ---------------------------
// ---------------------------


// ---------------------------
//     Login operations
// ---------------------------
void login(Admin *admin) {
    while (1) {
        clearScreen();
        printf("=== Login ===\n\n");
        char user[50], pass[50];
        printf("Username: ");
        scanf("%49s", user);
        printf("Password: ");
        scanf("%49s", pass);

        if (strcmp(user, admin->username) == 0 && salted_hash(pass) == admin->pass_hash) {
            clearScreen();
            printf(GREEN "\nLogin successful. Welcome, %s!" RESET "\n\n", admin->username);
            usleep(1500000);
            break;
        } else {
            clearScreen();
            printf(RED "\nInvalid username or password. Try again." RESET "\n");
            usleep(1500000);
        }
    }
}

void forgot_password(Admin *admin) {
    char user[50], answer[100];
    clearScreen();
    printf("=== Forgot Password ===\n\n");
    printf("Enter username: ");
    scanf("%49s", user);

    if (strcmp(user, admin->username) != 0) {
        clearScreen();
        printf(RED "\nUser not found." RESET "\n");
        usleep(1500000);
        return;
    }

    printf("%s\n", admin->sec_question);
    printf("Answer: ");
    scanf("%99s", answer);

    if (salted_hash(answer) == admin->sec_answer_hash) {
        printf(GREEN "Verification successful." RESET "\n");
        usleep(1000000);
        change_password(admin);
    }
    else {
        printf(RED "Incorrect answer." RESET "\n");
        usleep(1500000);
    }
}
// ---------------------------
// ---------------------------

// ---------------------------
//       Big Helpers
// ---------------------------
void update_inventory_batch(const char *med_name, const char *expiry, int sold_qty) {
    FILE *fp = fopen(INVENTORY_PATH, "rb");
    if (!fp) { printf(RED "Error: Could not open inventory file." RESET "\n"); return; }
    FILE *temp = fopen(INVENTORY_TMP_PATH, "wb");
    if (!temp) { fclose(fp); printf(RED "Error: Could not create temp file." RESET "\n"); return; }

    Medicine m;
    while (fread(&m, sizeof(Medicine), 1, fp) == 1) {
        if (strcmp(m.name, med_name) == 0 && strcmp(m.expire_date, expiry) == 0) {
            m.total_units -= sold_qty;
            if (m.total_units < 0) m.total_units = 0;
        }
        fwrite(&m, sizeof(Medicine), 1, temp);
    }

    fclose(fp);
    fclose(temp);
    remove(INVENTORY_PATH);
    rename(INVENTORY_TMP_PATH, INVENTORY_PATH);
}

int fifo_consume(const char *med_name, int qty) {
    if (qty <= 0) return 0;

    FILE *fp = fopen(INVENTORY_PATH, "rb");
    if (!fp) return 0;

    Medicine batches[1000];
    int cnt = 0;
    Medicine m;

    while (fread(&m, sizeof(Medicine), 1, fp) == 1) {
        if (strcmp(m.name, med_name) == 0) {
            if (cnt < 1000) {
                batches[cnt++] = m;
            }
        }
    }
    fclose(fp);

    if (cnt == 0) {
        return 0;
    }

    qsort(batches, cnt, sizeof(Medicine), cmp_expire_asc);

    int remaining = qty;
    for (int i = 0; i < cnt && remaining > 0; i++) {
        int take = batches[i].total_units < remaining ? batches[i].total_units : remaining;
        if (take > 0) {
            update_inventory_batch(med_name, batches[i].expire_date, take);
            remaining -= take;
        }
    }
    int sold = qty - remaining;
    return sold;
}


int find_med(char* med, int n) {
    while (1) {
        scanf(" %[^\n]", med);
        if(strcmp(med,"quit")==0) return 1;
        int x = 0;
        for (int i = 0; i < (int)strlen(med); i++)
            if (med[i] == ' ')
                med[i] = '_';

        char list_directory[200] = (MEDLIST_PATH);
        char list[100], store_list[10000][100];

        list_directory[strlen(list_directory)-strlen("med_name.txt") - 2] = (med[0] >= 'A' && med[0] <= 'Z') ? med[0] : (med[0] - ('a' - 'A'));


        FILE *med_list = fopen(list_directory, "r");
        if (med_list == NULL) {
            printf("\nInvalid Name.\nEnter the medicine name again: ");
            continue;
        }

        while (fgets(list, sizeof(list), med_list)) {
            int miss_matched = 0;
            for (int i = 0; i < (int)strlen(med); i++) {
                if (med[i] - list[i] == 0 ||
                    med[i] - list[i] == 'a' - 'A' ||
                    med[i] - list[i] == -('a' - 'A') ||
                    (med[i]=='_' && list[i]=='-'))
                    continue;
                miss_matched=1;
            }
            if (!miss_matched) {
                strcpy(store_list[x], list);
                store_list[x][strcspn(store_list[x], "\n")] = 0;
                x++;
                printf("\n%d: %s", x, store_list[x - 1]);
                 ;
            }
        }
        fclose(med_list);

        if (x == 0) {
            printf("\n\nNo Medicine with that name was found.\nEnter the medicine name again: ");
            continue;
        }

        int choice;
        char input[20];
        printf("\n\nType the corresponding number with the medicine of your choice: ");

        while (1) {
            int flagged = 0;
            scanf(" %[^\n]", input);
            getchar();
            if(strcmp(input, "quit")==0) return 1;

            for(int i = 0; i < (int)strlen(input); i++) {
                if (!(input[i]>='0' && input[i]<='9')) {
                    flagged = 1;
                    break;
                }
            }

            if(!flagged) {
                choice = atoi(input);
                if (choice >= 1 && choice <= x) {
                    break;
                }
            } else {
                printf("\nInvalid Choice. Type again: ");
                continue;
            }
        }

        strcpy(med, store_list[choice - 1]);
        return 0;
    }
}
// ---------------------------
// ---------------------------


// ============================
// Add / Update Medicine
// ============================

void addOrUpdateMedicine() {

    clearScreen();
    printf("=== Add Medicine ===\n\n");

    while(1){
    Medicine newMedicine, temp;
    FILE *inventoryFile = NULL, *tempInventory = NULL;
    int found = 0, check;




    printf("Enter Medicine Name (Type \"quit\" to return to Medicine menu): ");
    check = find_med(newMedicine.name, sizeof(newMedicine.name));
    if(check==1) return;

    char input[20];
    int year, month, day;

    while (1) {
        printf("Enter Expiration Date (YYYY-MM-DD): ");
        scanf(" %19[^\n]", input);

        if (strlen(input) == 10 &&
            Isdigit(input[0]) && Isdigit(input[1]) && Isdigit(input[2]) && Isdigit(input[3]) &&
            input[4] == '-' &&
            Isdigit(input[5]) && Isdigit(input[6]) &&
            input[7] == '-' &&
            Isdigit(input[8]) && Isdigit(input[9]))
        {
            year  = atoi(&input[0]);
            month = atoi(&input[5]);
            day   = atoi(&input[8]);

            if ((year >= 1) && (month >= 1 && month <= 12) && (day >= 1 && day <= 31)) {
                snprintf(newMedicine.expire_date, sizeof(newMedicine.expire_date), "%04d-%02d-%02d", year, month, day);
                break;
            }
        }

        printf("\nInvalid input\nTry Again.\n");
    }

    printf("Enter unit price: ");
    scanf("%f", &newMedicine.unit_price);

    printf("Enter Total Units for Stock: ");
    scanf("%d", &newMedicine.total_units);

    srand((unsigned)time(0));
    newMedicine.id = rand() % 10000 + 1;
    getCurrentDateTime(newMedicine.entry_datetime);

    if (!log_append_medicine(&newMedicine)) {
        printf(RED "Error logging medicine update." RESET "\n");
        return;
    }

    inventoryFile = fopen(INVENTORY_PATH, "rb");
    tempInventory = fopen(INVENTORY_TMP_PATH, "wb");
    if (!tempInventory) {
        printf(RED "Error creating temp inventory file!" RESET "\n");
        if (inventoryFile) fclose(inventoryFile);
        return;
    }

    if (inventoryFile) {
        while (fread(&temp, sizeof(Medicine), 1, inventoryFile) == 1) {
            if (strcmp(temp.name, newMedicine.name) == 0 &&
                strcmp(temp.expire_date, newMedicine.expire_date) == 0) {
                temp.total_units += newMedicine.total_units;
                found = 1;
            }
            fwrite(&temp, sizeof(Medicine), 1, tempInventory);
        }
        fclose(inventoryFile);
    }

    if (!found) {
        fwrite(&newMedicine, sizeof(Medicine), 1, tempInventory);
    }

    fclose(tempInventory);
    remove(INVENTORY_PATH);
    rename(INVENTORY_TMP_PATH, INVENTORY_PATH);

    printf(GREEN "\nMedicine '%s' (exp:%s) updated successfully." RESET "\n\n", newMedicine.name, newMedicine.expire_date);

}
        clearScreen();
}
// ============================
// ============================


// ============================
//           Stock
// ============================
void viewCurrentStock() {
    FILE *inv = fopen(INVENTORY_PATH, "rb");
    if (!inv) {
        printf(RED "\nNo inventory found." RESET "\n");
        usleep(1000000);
        return;
    }

    Medicine meds[1000];
    int count = 0;

    while (fread(&meds[count], sizeof(Medicine), 1, inv) == 1) {
        count++;
    }
    fclose(inv);

    while(1){
    int option;

    clearScreen();
    printf("=== Current Stock ===\n\n");

    printf("1. Show medicine quantity\n");
    printf("2. Show medicine batch\n");
    printf("0. Back to Medicine menu\n");
    printf("Enter choice: ");
    scanf("%d", &option);
    getchar();

    if(option!=1 && option!=2 && option!=0)
    {
          clearScreen();
          printf(RED "Invalid Input\n" RESET);
          usleep(2000000);
          continue;
    }


    int used[1000] = {0};
    clearScreen();
    printf("=== Current Stock ===\n\n");
    if(option==1){
        printf("+----------------------------------------------+---------+---------+\n");
        printf("| Medicine Name                                | Units   | Price   |\n");
        printf("+----------------------------------------------+---------+---------+\n");

        for (int i = 0; i < count; i++) {
            if (used[i] || meds[i].total_units==0) continue;
            int total = meds[i].total_units;

            for (int j = i + 1; j < count; j++) {
                if (!used[j] && strcmp(meds[i].name, meds[j].name) == 0) {
                    total += meds[j].total_units;
                    used[j] = 1;
                    }
                }
                printf("| %-44s | %-7d | %-7.2f |\n", meds[i].name, total, meds[i].unit_price);
                printf("+----------------------------------------------+---------+---------+\n");
                 ;
            }

        }

    else if(option==2){
        printf("+------+----------------------------------------------+---------+--------------+---------+\n");
        printf("| ID   | Medicine Name                                | Units   | Expiry Date  | Price   |\n");
        printf("+------+----------------------------------------------+---------+--------------+---------+\n");

        for (int i = 0; i < count; i++) {
            if (used[i] || meds[i].total_units==0) continue;
                printf("| %-4d | %-44s | %-7d | %-12s | %-7.2f |\n", meds[i].id, meds[i].name, meds[i].total_units, meds[i].expire_date, meds[i].unit_price);
                printf("+------+----------------------------------------------+---------+--------------+---------+\n");
                 ;

        }
    }
    else if(option==0) return;

    printf("\nPress Enter to continue...");
    getchar();

    }
}


void SearchMedInfo()  {
    char med[100], directory[140]="Data\\Letter_X\\";
    clearScreen();
    printf("=== Search Medicine Information ===\n\n");
    printf("Enter the medicine name to see the details: ");
    if (find_med(med,sizeof(med))) return;

    directory[12]=med[0];
    strcat(directory,med);
    strcat(directory,".txt");

    char line[500];
    FILE *info;
    info = fopen(directory, "r");
    if (info == NULL) {
        printf(RED "Medicine information file not found." RESET "\n");
        usleep(1500000);
        return;
    }

    clearScreen();
    printf("=== Medicine Information: %s ===\n\n", med);
    while (fgets(line, sizeof(line), info)){
        printf("%s", line);
         ;
    }

    fclose(info);
    printf("\nPress Enter to continue...");
    getchar();

}
// ============================
// ============================



// ============================
//           Logs
// ============================

void viewInventoryLog() {
    FILE *file;
    unsigned char tag;
    Medicine medicine;
    int option;

    while(1){
    clearScreen();
    printf("=== Inventory Log ===\n\n");
    printf("1. Show full log\n");
    printf("2. Search by medicine name\n");
    printf("0. Back to Medicine menu\n");
    printf("Enter choice: ");
    scanf("%d", &option);
    getchar();

    file = fopen(LOG_PATH, "rb");
    if (file == NULL) {
        printf(RED "\nNo log found." RESET "\n");
        usleep(1500000);
        return;
    }

    if (option == 1) {
        clearScreen();
        printf("=== Full Inventory Log ===\n\n");
        printf("----------------------------------------------------------------------------------------------------------------------------------\n");
        printf("| ID         | Medicine Name                                | Expire Date  | Total Units    | Entry Date & Time                  |\n");
        printf("----------------------------------------------------------------------------------------------------------------------------------\n");

        while (fread(&tag, 1, 1, file) == 1) {
            if (tag == 'M') {
                if (fread(&medicine, sizeof(Medicine), 1, file) != 1) break;
                printf("| %-10d | %-44s | %-12s | %-14d | %-34s |\n",
                       medicine.id, medicine.name, medicine.expire_date,
                       medicine.total_units, medicine.entry_datetime);
                printf("----------------------------------------------------------------------------------------------------------------------------------\n");
            } else if (tag == 'S') {
                Sale s; if (fread(&s, sizeof(Sale), 1, file) != 1) break;
                continue;
            } else {
                break;
            }
             ;
        }
    } else if (option == 2) {
        clearScreen();
        printf("=== Search Medicine Log ===\n\n");
        char searchName[50];
        printf("Enter medicine name to search: ");
        if (find_med(searchName, sizeof(searchName))==1){ fclose(file); return; }

        int found = 0;
        printf("\nSearch Results for '%s':\n", searchName);
        printf("----------------------------------------------------------------------------------------------------------------------------------\n");
        printf("| ID         | Medicine Name                                | Expire Date  | Total Units    | Entry Date & Time                  |\n");
        printf("----------------------------------------------------------------------------------------------------------------------------------\n");

        while (fread(&tag, 1, 1, file) == 1) {
            if (tag == 'M') {
                if (fread(&medicine, sizeof(Medicine), 1, file) != 1) break;
                if (strcmp(medicine.name, searchName) == 0) {
                    printf("| %-10d | %-44s | %-12s | %-14d | %-34s |\n",
                           medicine.id, medicine.name, medicine.expire_date,
                           medicine.total_units, medicine.entry_datetime);
                    printf("----------------------------------------------------------------------------------------------------------------------------------\n");
                    found = 1;
                }
            } else if (tag == 'S') {
                Sale s; if (fread(&s, sizeof(Sale), 1, file) != 1) break;
            } else {
                break;
            }

             ;
        }
        if (!found) printf("\nNo records found for '%s'.\n", searchName);
    }

    else if(option==0) break;

    else {
        printf(RED "\nInvalid choice." RESET "\n");
    }

    fclose(file);
    printf("\nPress Enter to continue...");
    getchar();
    }
}

void CustomerLog() {
    FILE *fp = fopen(LOG_PATH, "rb");
    if (!fp) {
        printf(RED "No sales log found." RESET "\n");
        usleep(1500000);
        return;
    }

    unsigned char tag;
    Sale sale;
    clearScreen();
    printf("=== Customer Log ===\n\n");
    printf("+---------------------+--------------------------+------------------+----------------------------------------------------+---------+\n");
    printf("| Date & Time         | Customer Name            | Phone            | Medicine                                           | Qty     |\n");
    printf("+---------------------+--------------------------+------------------+----------------------------------------------------+---------+\n");

    while (fread(&tag, 1, 1, fp) == 1) {
        if (tag == 'S') {
            if (fread(&sale, sizeof(Sale), 1, fp) != 1) break;
            printf("| %-19s | %-24s | %-16s | %-50s | %-7d |\n",
                   sale.datetime,
                   sale.customerName,
                   sale.phone,
                   sale.medName,
                   sale.quantity);
            printf("+---------------------+--------------------------+------------------+----------------------------------------------------+---------+\n");

        } else if (tag == 'M') {
            Medicine m; if (fread(&m, sizeof(Medicine), 1, fp) != 1) break;
        } else {
            break;
        }
         ;
    }
    fclose(fp);

    printf("\nPress Enter to continue...");
    getchar();
}

void SalesLog() {
    FILE *log = fopen(LOG_PATH, "rb");
    if (!log) {
        printf(RED "\nNo sales log found." RESET "\n");
        usleep(1500000);
        return;
    }

    unsigned char tag;
    Sale sale;

    clearScreen();
    printf("=== Sales Log ===\n\n");
    printf("+----------------------------------------------------+-------------+---------------------+\n");
    printf("| Medicine                                           | Qty (batch) | Date & Time Sold    |\n");
    printf("+----------------------------------------------------+-------------+---------------------+\n");

    while (fread(&tag, 1, 1, log) == 1) {
        if (tag == 'S') {
            if (fread(&sale, sizeof(Sale), 1, log) != 1) break;
            printf("| %-50s | %-5d       | %-19s |\n",
                   sale.medName, sale.quantity,
                   sale.datetime);
            printf("+----------------------------------------------------+-------------+---------------------+\n");
        } else if (tag == 'M') {
            Medicine m; if (fread(&m, sizeof(Medicine), 1, log) != 1) break;
        } else {
            break;
        }
         ;
    }

    fclose(log);
    printf("\nPress Enter to continue...");
    getchar();
}

void LogsMenu() {
    int choice;
    do {
        clearScreen();
        printf("=== Logs System ===\n\n");
        printf("1) Inventory Log\n");
        printf("2) Customer Log\n");
        printf("3) Sales Log\n");
        printf("0) Back to Medicine Menu\n");
        printf("Choice: ");
        scanf("%d", &choice);
        getchar();

        switch (choice) {
            case 1: viewInventoryLog(); break;
            case 2: CustomerLog(); break;
            case 3: SalesLog(); break;
            case 0: break;
            default: printf(RED "Invalid choice!\n" RESET); usleep(1500000);
        }
    } while (choice != 0);
}
// ============================
// ============================



// ============================
// Check Expiry
// ============================

void CheckExpiry() {
    while(1){
    FILE *file;
    Medicine medicine;
    char currentDateTime[20], currentDate[11];
    int option;

    getCurrentDateTime(currentDateTime);
    strncpy(currentDate, currentDateTime, 10);
    currentDate[10] = '\0';

    file = fopen(INVENTORY_PATH, "rb");
    if (!file) {
        printf(RED "\nNo inventory data found.\n" RESET);
        usleep(1500000);
        return;
    }

    Medicine medicines[1000];
    int count = 0;

    while (fread(&medicine, sizeof(Medicine), 1, file) == 1) {
        if (count < 1000) {
            medicines[count++] = medicine;
        }
    }
    fclose(file);

    clearScreen();
    printf("=== Check Expiry ===\n\n");
    printf("1. Show expiry list\n");
    printf("2. Remove expired medicines from INVENTORY\n");
    printf("0. Back to Medicine menu\n");
    printf("Enter choice: ");
    scanf("%d", &option);
    getchar();

    qsort(medicines, count, sizeof(Medicine), cmp_expire_asc);

    if (option == 1) {
        clearScreen();
        printf("=== Expiry Status ===\n\n");
        printf("---------------------------------------------------------------------\n");
        printf("|  ID  | Medicine Name                                | Expire Date |\n");
        printf("---------------------------------------------------------------------\n");

        struct tm curr_tm; parseDate(currentDate, &curr_tm); time_t curr_time = mktime(&curr_tm);

        for (int i = 0; i < count; i++) {
            struct tm exp_tm; parseDate(medicines[i].expire_date, &exp_tm); time_t exp_time = mktime(&exp_tm);
            double diff_days = difftime(exp_time, curr_time) / (60 * 60 * 24);

            if (diff_days < 0) {
                printf("|" RED " %-5d" RESET "|" RED " %-44s " RESET "|" RED " %-11s " RESET "|\n" RESET, medicines[i].id, medicines[i].name, medicines[i].expire_date);
            } else if (diff_days >= 730) {
                printf("|" GREEN " %-5d" RESET "|" GREEN " %-44s " RESET "|" GREEN " %-11s " RESET "|\n" RESET, medicines[i].id, medicines[i].name, medicines[i].expire_date);
            } else {
                printf("|" YELLOW " %-5d" RESET "|" YELLOW " %-44s " RESET "|" YELLOW " %-11s " RESET "|\n" RESET, medicines[i].id, medicines[i].name, medicines[i].expire_date);
            }
            printf("---------------------------------------------------------------------\n");
             ;
        }

        printf("\nPress Enter to continue...");
        getchar();

    } else if (option == 2) {
        FILE *tempFile = fopen(INVENTORY_TMP_PATH, "wb");
        if (!tempFile) {
            printf(RED "\nUnable to open temporary file.\n"RESET);
            usleep(1500000);
            return;
        }

        int removed = 0;
        struct tm curr_tm; parseDate(currentDate, &curr_tm); time_t curr_time = mktime(&curr_tm);

        for (int i = 0; i < count; i++) {
            struct tm exp_tm; parseDate(medicines[i].expire_date, &exp_tm); time_t exp_time = mktime(&exp_tm);
            double diff_days = difftime(exp_time, curr_time) / (60 * 60 * 24);

            if (diff_days >= 0) {
                fwrite(&medicines[i], sizeof(Medicine), 1, tempFile);
            } else {
                Medicine logMed = medicines[i];
                getCurrentDateTime(logMed.entry_datetime);
                log_append_medicine(&logMed);
                removed++;
            }
        }

        fclose(tempFile);
        remove(INVENTORY_PATH);
        rename(INVENTORY_TMP_PATH, INVENTORY_PATH);

        printf(GREEN "\nExpired medicine batches removed from INVENTORY: %d" RESET "\n", removed);
        usleep(2000000);
    }

    else if(option==0) return;

    else {
        printf(RED "\nInvalid choice." RESET "\n");
        usleep(1500000);
    }

    }
}

// ============================
// ============================



// ============================
// Sales Entry
// ============================

void SellEntry() {
    char customerName[50], phone[20], med[100];
    int quantity;

    clearScreen();
    printf("=== Sales Entry ===\n\n");
    printf("Enter Customer Name: ");
    scanf(" %[^\n]", customerName);
    printf("Enter Customer Phone: ");
    scanf(" %[^\n]", phone);

    clearScreen();
    printf("=== Sales Entry ===\n\n");
    while (1) {
        printf("Customer: %s | Phone: %s\n\n", customerName, phone);
        printf("Enter Medicine Name (Type 'quit' to finish): ");
        if (find_med(med, sizeof(med)) == 1) return;
        if (strcmp(med, "quit") == 0) break;

        FILE *inventoryFile = fopen(INVENTORY_PATH, "rb");
        if (!inventoryFile) { printf(RED "No inventory file found." RESET "\n"); continue; }

        Medicine batches[1000];
        int count = 0;
        Medicine tmp;
        while (fread(&tmp, sizeof(Medicine), 1, inventoryFile) == 1) {
            if (strcmp(tmp.name, med) == 0 && tmp.total_units > 0) {
                if (count < 1000) {
                    batches[count++] = tmp;
                }
            }
        }
        fclose(inventoryFile);

        if (count == 0) {
            printf(RED "Medicine not found in stock: %s" RESET "\n", med);
            usleep(1500000);
            continue;
        }

        qsort(batches, count, sizeof(Medicine), cmp_expire_asc);

        printf("\nAvailable batches for %s (Earliest first):\n", med);
        for (int i = 0; i < count; i++) {
            printf("%2d) Expiry: %s | Units: %d | Batch ID: %d | Price: %.2f\n",
                   i + 1, batches[i].expire_date, batches[i].total_units, batches[i].id, batches[i].unit_price);
        }
        printf(" 0) FIFO (auto from earliest expiry)\n");

        int choice = -1;
        printf("Select option: ");
        scanf("%d", &choice);

        if (choice < 0 || choice > count) {
            printf(RED "Invalid choice." RESET "\n");
            usleep(1500000);
            continue;
        }

        printf("Enter quantity to sell: ");
        scanf("%d", &quantity);
        if (quantity <= 0) { printf(RED "Invalid quantity." RESET "\n"); usleep(1500000); continue; }

        int sold = 0;
        char used_expiry[11] = "";

        if (choice == 0) {
            sold = fifo_consume(med, quantity);
            if (sold > 0) strcpy(used_expiry, "(FIFO)");
        } else {
            int idx = choice - 1;
            if (quantity > batches[idx].total_units) {
                printf(RED "Only %d units available in selected batch!" RESET "\n", batches[idx].total_units);
                printf("\nDo you want to buy %d instead (press y/n for answer)?\nChoice:", batches[idx].total_units);

                here:
                char ch = getchar();
                if (ch == 'y')
                    quantity = batches[idx].total_units;
                else if (ch == 'n')
                    continue;
                else
                    goto here;
            }
            if (quantity > 0) {
                update_inventory_batch(med, batches[idx].expire_date, quantity);
                sold = quantity;
                strncpy(used_expiry, batches[idx].expire_date, sizeof(used_expiry));
                used_expiry[sizeof(used_expiry) - 1] = '\0';
            }
        }

        if (sold > 0) {
            Sale sale; memset(&sale, 0, sizeof(sale));
            getCurrentDateTime(sale.datetime);
            strncpy(sale.customerName, customerName, sizeof(sale.customerName)); sale.customerName[sizeof(sale.customerName) - 1] = '\0';
            strncpy(sale.phone, phone, sizeof(sale.phone)); sale.phone[sizeof(sale.phone) - 1] = '\0';
            strncpy(sale.medName, med, sizeof(sale.medName)); sale.medName[sizeof(sale.medName) - 1] = '\0';
            strncpy(sale.expire_date, used_expiry, sizeof(sale.expire_date)); sale.expire_date[sizeof(sale.expire_date) - 1] = '\0';
            sale.quantity = sold;

            if (!log_append_sale(&sale)) {
                printf(RED "Error writing to log." RESET "\n");
            } else {
                printf(GREEN "Sold %d units of %s %s." RESET "\n", sold, med, (choice == 0 ? "(FIFO)" : ""));
            }
        } else {
            printf(RED "No units sold." RESET "\n");
            usleep(1500000);
        }
    }
    clearScreen();
}

// ============================
// ============================


// ============================
//      Coustomer Request
// ============================
void CustomerRequest() {
    char medicine[50];
    char choice;
    FILE *file;
    int check;

    do {
        clearScreen();
        printf("=== Customer Medicine Request System ===\n\n");
        printf("1. Add Request\n");
        printf("2. Show Requests\n");
        printf("3. Delete Request\n");
        printf("0. Back to Medicine menu\n");
        printf("Enter choice: ");
        scanf(" %c", &choice);

        if (choice == '1') {
    clearScreen();
    printf("=== Add Request ===\n\n");
    printf("Enter medicine name: ");
    check = find_med(medicine, sizeof(medicine));
    if (check == 1) continue;

    // Check for duplicates
    int exists = 0;
    file = fopen(CUSTOMER_REQUEST, "r");
    if (file != NULL) {
        char line[50];
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = 0; // Remove newline
            if (strcasecmp(line, medicine) == 0) {
                exists = 1;
                break;
            }
        }
        fclose(file);
    }

    if (exists) {
        printf(RED "Medicine '%s' already exists in the request list. Ignored." RESET "\n", medicine);
        usleep(1500000);
    } else {
        file = fopen(CUSTOMER_REQUEST, "a");
        if (file == NULL) {
            printf(RED "Error opening file!" RESET "\n");
        } else {
            fprintf(file, "%s\n", medicine);
            fclose(file);
            printf(GREEN "Medicine '%s' added to request list." RESET "\n", medicine);
            usleep(1500000);
        }
    }
}

        else if (choice == '2') {
            clearScreen();
            printf("=== Requested Medicines ===\n\n");
            file = fopen(CUSTOMER_REQUEST, "r");
            if (file == NULL) {
                printf("No requests found.\n");
            } else {
                char line[50];
                int i = 1;
                while (fgets(line, sizeof(line), file)) {
                    line[strcspn(line, "\n")] = 0;
                    printf("%d. %s\n", i, line);
                    i++;
                }
                fclose(file);
            }
            printf("\nPress Enter to continue...");
            getchar();
            getchar();
        }
        else if (choice == '3') {
            clearScreen();
            printf("=== Delete Request ===\n\n");
            file = fopen(CUSTOMER_REQUEST, "r");
            if (file == NULL) {
                printf("No requests found.\n");
                usleep(1500000);
                continue;
            }

            char requests[100][50];
            int count = 0;
            while (fgets(requests[count], sizeof(requests[count]), file)) {
                requests[count][strcspn(requests[count], "\n")] = 0;
                count++;
            }
            fclose(file);

            if (count == 0) {
                printf("No requests to delete.\n");
                usleep(1500000);
                continue;
            }

            printf("Requested Medicines:\n");
            for (int i = 0; i < count; i++) {
                printf("%d. %s\n", i + 1, requests[i]);
            }

            int delNum;
            printf("Enter the number of the request to delete: ");
            scanf("%d", &delNum);

            if (delNum < 1 || delNum > count) {
                printf(RED "Invalid number!" RESET "\n");
                usleep(1500000);
                continue;
            }

            for (int i = delNum - 1; i < count - 1; i++) {
                strcpy(requests[i], requests[i + 1]);
            }
            count--;

            file = fopen(CUSTOMER_REQUEST, "w");
            if (file == NULL) {
                printf(RED "Error opening file!" RESET "\n");
                usleep(1500000);
                continue;
            }
            for (int i = 0; i < count; i++) {
                fprintf(file, "%s\n", requests[i]);
            }
            fclose(file);

            printf(GREEN "Request deleted successfully." RESET "\n");
            usleep(1500000);
        }
        else if (choice == '0') return;
        else {
            printf(RED "Invalid choice!" RESET "\n");
            usleep(1500000);
        }

    } while (1);
}
// ============================
// ============================



// ============================
// Main Function
// ============================

int main() {
    enable_utf8_console();
    printf(BASE_COLOR);

    logo();


    int f_login;
    Admin admin;
    f_login=init_admin_file();
    if (!read_admin(&admin)) {
        printf(RED "Error reading admin file." RESET "\n");
        return 1;
    }

    int choice;

    while(1){

        clearScreen();
        l_portal:
        if(f_login){
            printf("Initialized with default credentials:\n");
            printf("Username: admin | Password: admin | Security answer: 1234\n\n");
        }
        printf("=== Pharmacy Admin System ===\n\n");
        printf("1) Login\n");
        printf("2) Forgot Password\n");
        printf("3) Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);
        getchar();

        if(choice == 1) {login(&admin); break;}
        if(choice == 2) {forgot_password(&admin); continue;}
        if(choice == 3) {clearScreen(); return 0;}
        else {printf(RED "Invalid option." RESET "\n"); usleep(1500000);}
    }

    do {
        clearScreen();
        printf("=== Pharmacy Management System ===\n\n");
        printf("1. Admin System\n");
        printf("2. Medicine Management System\n");
        printf("3. Log out\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar();

        switch (choice) {
            case 1: {
                int admin_choice;
                do {
                    clearScreen();
                    printf("=== Admin Options ===\n\n");
                    printf("1) Change Username\n");
                    printf("2) Change Password\n");
                    printf("3) Change Security question\n");
                    printf("0) Back to Main Menu\n");
                    printf("Choice: ");
                    scanf("%d", &admin_choice);

                    switch (admin_choice) {
                        case 1: change_username(&admin); break;
                        case 2: change_password(&admin); break;
                        case 3: change_security(&admin); break;
                        case 0: break;
                        default: printf(RED "Invalid choice." RESET "\n"); usleep(1500000);
                    }
                } while (admin_choice != 0);
            }
            break;

            case 2: {
                int med_choice;
                do {
                    clearScreen();
                    printf("=== Medicine Management System ===\n\n");
                    printf("1. Add Medicine\n");
                    printf("2. View Current Stock\n");
                    printf("3. Search Medicine Information\n");
                    printf("4. Check Logs\n");
                    printf("5. Check Expired Items\n");
                    printf("6. Sale Entry\n");
                    printf("7. Customer Request\n");
                    printf("0. Back to Main Menu\n");
                    printf("Enter your choice: ");
                    scanf("%d", &med_choice);
                    getchar();

                    switch (med_choice) {
                        case 1: addOrUpdateMedicine(); break;
                        case 2: viewCurrentStock(); break;
                        case 3: SearchMedInfo(); break;
                        case 4: LogsMenu(); break;
                        case 5: CheckExpiry(); break;
                        case 6: SellEntry(); break;
                        case 7: CustomerRequest(); break;
                        case 0: break;
                        default: printf(RED "Invalid choice." RESET "\n"); usleep(1500000);
                    }
                } while (med_choice != 0);
            }
            break;

            case 3:
                clearScreen();
                printf("Are you sure you want to log out? (press y to confirm. press any key to cancel)\n");
                char choice=getchar();
                if(choice=='y')
                {   clearScreen();
                    printf(GREEN "Logging out" RESET);
                    usleep(2000000);
                    clearScreen();
                    goto l_portal;
                }

                break;

            default:
                printf(RED "Invalid choice." RESET "\n");
                usleep(1500000);
        }

    } while (choice != 3);

    return 0;
}
