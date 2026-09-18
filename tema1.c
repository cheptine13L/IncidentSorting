#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct unit {
    int id;
    char type;
    int availability;
};

struct incident {
    int id;
    char priority[7];
    char *description; // dynamically allocated
    char status[11];   // queued, intervened, or solved
    struct incident *prev; // circular link
    struct incident *next;
};

struct intervention {
    struct incident *incident;
    struct unit *unit;
    struct intervention *prev; // circular list link
    struct intervention *next;
};

// Generic node for queues and stack
struct node {
    void *data;
    struct node *next;
    struct node *prev;
};

struct queue {
    struct node *front;
    struct node *back;
};

struct stack {
    struct node *top;
};

struct system {
    struct unit *units;
    int num_units;
    struct incident *incidents;      // sentinel for the list
    struct intervention *interventions; // sentinel for active interventions
    struct queue q_high;
    struct queue q_med;
    struct queue q_low;
    struct queue q_units;            // available units queue
    struct stack hist;               // undo history
};

// Adds a new incident to the double-linked circular list and priority queue
void add_incident(struct system *sys, int id, char *prio, char *desc) {
    struct incident *incnew = malloc(sizeof(struct incident));
    if(!incnew) return;

    incnew->id = id;
    strcpy(incnew->priority, prio);
    incnew->description = malloc(strlen(desc) + 1); // exactly strlen + 1
    if(incnew->description) strcpy(incnew->description, desc);
    strcpy(incnew->status, "queued");

    // Insert into global circular list using sentinel
    struct incident *sent = sys->incidents;
    struct incident *last = sent->prev;
    last->next = incnew;
    incnew->prev = last;
    incnew->next = sent;
    sent->prev = incnew;

    // Create node for priority queue
    struct node *nodeq = malloc(sizeof(struct node));
    if(!nodeq) return;
    nodeq->data = incnew;
    nodeq->next = NULL;
    nodeq->prev = NULL;

    struct queue *target = NULL;
    if(strcmp(prio, "high") == 0) target = &sys->q_high;
    else if(strcmp(prio, "medium") == 0) target = &sys->q_med;
    else target = &sys->q_low;

    // Standard FIFO enqueue
    if(target->front == NULL) {
        target->front = nodeq;
        target->back = nodeq;
    } else {
        target->back->next = nodeq;
        target->back = nodeq;
    }
}

// Matches the highest priority incident with the first available unit
void dispatch(struct system *sys, FILE *fout) {
    // Check if there are any incidents or units available
    if(sys->q_high.front == NULL && sys->q_med.front == NULL && sys->q_low.front == NULL) {
        fprintf(fout, "INVALID OPERATION! ERROR 404\n");
        return;
    }
    if(sys->q_units.front == NULL) {
        fprintf(fout, "INVALID OPERATION! ERROR 404\n");
        return;
    }

    // Determine target queue based on priority
    struct queue *target = NULL;
    if(sys->q_high.front != NULL) target = &sys->q_high;
    else if(sys->q_med.front != NULL) target = &sys->q_med;
    else target = &sys->q_low;

    // Dequeue incident
    struct incident *inc = (struct incident *)target->front->data;
    struct node *old_node = target->front;
    target->front = target->front->next;
    if(target->front == NULL) target->back = NULL;
    free(old_node);

    // Dequeue available unit
    struct unit *un = (struct unit *)sys->q_units.front->data;
    struct node *old_un_node = sys->q_units.front;
    sys->q_units.front = sys->q_units.front->next;
    if(sys->q_units.front == NULL) sys->q_units.back = NULL;
    free(old_un_node);

    // Update status and create intervention link
    strcpy(inc->status, "intervened");
    un->availability = 0;

    struct intervention *intnew = malloc(sizeof(struct intervention));
    intnew->incident = inc;
    intnew->unit = un;

    // Add to circular intervention list using sentinel
    struct intervention *sent = sys->interventions;
    struct intervention *last = sent->prev;
    last->next = intnew;
    intnew->prev = last;
    intnew->next = sent;
    sent->prev = intnew;

    // Push to history stack for undo
    struct node *nodes = malloc(sizeof(struct node));
    nodes->data = intnew;
    nodes->next = sys->hist.top;
    nodes->prev = NULL;
    sys->hist.top = nodes;
}

// Reverts the last intervention: incident returns to queue, unit becomes free
void undo_last_dispatch(struct system *sys, FILE *fout) {
    struct node *cur = sys->hist.top;
    while(cur != NULL) {
        struct intervention *interv = (struct intervention *)cur->data;
        // Only undo interventions that haven't been solved yet
        if(strcmp(interv->incident->status, "intervened") == 0) {
            strcpy(interv->incident->status, "queued");
            
            struct queue *target = NULL;
            if(strcmp(interv->incident->priority, "high") == 0) target = &sys->q_high;
            else if(strcmp(interv->incident->priority, "medium") == 0) target = &sys->q_med;
            else target = &sys->q_low;

            // Incident returns to the fornt of its queue
            struct node *nodeq = malloc(sizeof(struct node));
            nodeq->data = interv->incident;
            nodeq->next = target->front;
            nodeq->prev = NULL;
            if(target->front == NULL) target->back = nodeq;
            target->front = nodeq;

            // Unit becomes available again (back of the units queue)
            interv->unit->availability = 1;
            struct node *nodeu = malloc(sizeof(struct node));
            nodeu->data = interv->unit;
            nodeu->next = NULL;
            nodeu->prev = NULL;
            if(sys->q_units.front == NULL) {
                sys->q_units.front = sys->q_units.back = nodeu;
            } else {
                sys->q_units.back->next = nodeu;
                sys->q_units.back = nodeu;
            }

            // Remove intervention from circular list
            interv->prev->next = interv->next;
            interv->next->prev = interv->prev;
            
            free(interv);
            sys->hist.top = cur->next;
            free(cur);
            return;
        }
        // Cleanup history stack if intervention was already solved
        struct node *old = cur;
        cur = cur->next;
        sys->hist.top = cur;
        free(old);
    }
    fprintf(fout, "INVALID OPERATION! ERROR 404\n");
}

// Updates incident to solved and releases the assigned unit
void solved_incident(struct system *sys, FILE *fout, int id) {
    struct incident *cur = sys->incidents->next;
    while(cur != sys->incidents) {
        if(cur->id == id) {
            if(strcmp(cur->status, "intervened") != 0) {
                fprintf(fout, "INVALID OPERATION! ERROR 404\n");
                return;
            }
            strcpy(cur->status, "solved");
            
            // Find the unit assigned to this incident and release it
            struct intervention *interv = sys->interventions->next;
            while(interv != sys->interventions) {
                if(interv->incident == cur) {
                    interv->unit->availability = 1;
                    struct node *nodeu = malloc(sizeof(struct node));
                    nodeu->data = interv->unit;
                    nodeu->next = NULL; nodeu->prev = NULL;
                    if(sys->q_units.front == NULL) sys->q_units.front = sys->q_units.back = nodeu;
                    else {
                        sys->q_units.back->next = nodeu;
                        sys->q_units.back = nodeu;
                    }
                    return;
                }
                interv = interv->next;
            }
            return;
        }
        cur = cur->next;
    }
    fprintf(fout, "INVALID OPERATION! ERROR 404\n");
}

// Counts and displays available units in the system
void check_units_availability(struct system *sys, FILE *fout) {
    int ctr = 0;
    struct node *cur = sys->q_units.front;
    while(cur != NULL) {
        ctr++;
        cur = cur->next;
    }
    fprintf(fout, "Number of available units: %d\n", ctr);
}

// Displays detailed info about a specific unit
void show_unit(struct system *sys, FILE *fout, int id) {
    for(int i = 0; i < sys->num_units; i++) {
        if(sys->units[i].id == id) {
            fprintf(fout, "Unit %d is type %c and is %s\n",
                sys->units[i].id, sys->units[i].type,
                sys->units[i].availability == 1 ? "available" : "unavailable");
            return;
        }
    }
    fprintf(fout, "INVALID OPERATION! ERROR 404\n");
}

// Displays detailed info about a specific incident
void show_incident(struct system *sys, FILE *fout, int id) {
    struct incident *cur = sys->incidents->next;
    while(cur != sys->incidents) {
        if(cur->id == id) {
            fprintf(fout, "Incident %d has %s priority, the following description: \"%s\" and is %s\n", 
                cur->id, cur->priority, cur->description, cur->status);
            return;
        }
        cur = cur->next;
    }
    fprintf(fout, "INVALID OPERATION! ERROR 404\n");
}

// Lists all active or solved interventions in the system
void show_interventions(struct system *sys, FILE *fout) {
    if(sys->interventions->next == sys->interventions) {
        fprintf(fout, "No intervention has been initiated\n");
        return;
    }
    struct intervention *cur = sys->interventions->next;
    while(cur != sys->interventions) {
        fprintf(fout, "Incident %d was assigned to unit %d, and has the following status: \"%s\"\n",
            cur->incident->id, cur->unit->id, cur->incident->status);
        cur = cur->next;
    }
}

// Memory allocation and initial system state setup
struct system *initialize_system(FILE *fin) {
    struct system *sys = malloc(sizeof(struct system));
    int n;
    if(fscanf(fin, "%d", &n) != 1) return NULL;
    sys->num_units = n;
    sys->units = malloc(n * sizeof(struct unit));
    for(int i=0; i<n; i++) {
        fscanf(fin, "%d %c", &sys->units[i].id, &sys->units[i].type);
        sys->units[i].availability = 1;
    }

    // Initialize list sentinels (ID 0 test incident)
    struct incident *sentinc = malloc(sizeof(struct incident));
    sentinc->id = 0;
    sentinc->description = malloc(5);
    strcpy(sentinc->description, "test");
    sentinc->prev = sentinc; sentinc->next = sentinc;
    sys->incidents = sentinc;

    struct intervention *sentint = malloc(sizeof(struct intervention));
    sentint->incident = NULL; sentint->unit = NULL;
    sentint->prev = sentint; sentint->next = sentint;
    sys->interventions = sentint;

    // Reset pointers for queues and history
    sys->q_high.front = sys->q_high.back = NULL;
    sys->q_med.front = sys->q_med.back = NULL;
    sys->q_low.front = sys->q_low.back = NULL;
    sys->q_units.front = sys->q_units.back = NULL;
    sys->hist.top = NULL;

    // Populate the initial available units queue
    for(int i=0; i<n; i++) {
        struct node *nodeu = malloc(sizeof(struct node));
        nodeu->data = &sys->units[i];
        nodeu->next = NULL; nodeu->prev = NULL;
        if(sys->q_units.front == NULL) sys->q_units.front = sys->q_units.back = nodeu;
        else {
            sys->q_units.back->next = nodeu;
            sys->q_units.back = nodeu;
        }
    }
    return sys;
}

// Cleanup of all allocated memory to avoid leaks 
void free_system(struct system *sys) {
    //  Free incident list (circular double-linked with sentinel)
    struct incident *curinc = sys->incidents->next;
    while(curinc != sys->incidents) {
        struct incident *nxt = curinc->next;
        free(curinc->description); // Free the dynamically allocated string
        free(curinc);              // Free the incident structure itself
        curinc = nxt;
    }
    free(sys->incidents->description); // Free sentinel description
    free(sys->incidents);              // Free sentinel node

    // Free intervention list (circular double-linked with sentinel)
    struct intervention *curint = sys->interventions->next;
    while(curint != sys->interventions) {
        struct intervention *nxt = curint->next;
        free(curint); // Free the intervention structure
        curint = nxt;
    }
    free(sys->interventions); // Free sentinel node

    // Free each queue's nodes manually (no for loop)
    struct node *n = NULL;
    struct node *nxt = NULL;

    // Free high priority queue nodes
    n = sys->q_high.front;
    while(n != NULL) {
        nxt = n->next;
        free(n);
        n = nxt;
    }

    // Free medium priority queue nodes
    n = sys->q_med.front;
    while(n != NULL) {
        nxt = n->next;
        free(n);
        n = nxt;
    }

    // Free low priority queue nodes
    n = sys->q_low.front;
    while(n != NULL) {
        nxt = n->next;
        free(n);
        n = nxt;
    }

    // Free units availability queue nodes
    n = sys->q_units.front;
    while(n != NULL) {
        nxt = n->next;
        free(n);
        n = nxt;
    }

    //  Free undo history stack nodes
    struct node *curst = sys->hist.top;
    while(curst != NULL) {
        struct node *st_nxt = curst->next;
        free(curst);
        curst = st_nxt;
    }

    // Final system cleanups
    free(sys->units); // Free the array of unit structures
    free(sys);        // Free the main system structure
}

int main() {
    FILE *fin = fopen("tema1.in", "r");
    FILE *fout = fopen("tema1.out", "w");

    struct system *sys = initialize_system(fin);
    int n_ops;
    if (fscanf(fin, "%d", &n_ops) != 1) return 0;
    for (int i = 0; i < n_ops; i++) {
        char op[35];
        fscanf(fin, "%s", op);

        if (strcmp(op, "ADD_INCIDENT") == 0) {
            int id;
            char prio[10], desc[256];
            fscanf(fin, "%d %s", &id, prio);
            char c;
            do {
                fscanf(fin, "%c", &c);
            } while (c != '"');

            int j = 0;
            while (1) {
                fscanf(fin, "%c", &c);
                if (c == '"') break;
                desc[j++] = c;
            }
            desc[j] = '\0'; 
            add_incident(sys, id, prio, desc);

        } else if (strcmp(op, "CHECK_UNITS_AVAILABILITY") == 0) {
            check_units_availability(sys, fout);
        } else if (strcmp(op, "DISPATCH") == 0) {
            dispatch(sys, fout);
        } else if (strcmp(op, "UNDO_LAST_DISPATCH") == 0) {
            undo_last_dispatch(sys, fout);
        } else if (strcmp(op, "SOLVED_INCIDENT") == 0) {
            int id; 
            fscanf(fin, "%d", &id);
            solved_incident(sys, fout, id);
        } else if (strcmp(op, "SHOW_UNIT") == 0) {
            int id; 
            fscanf(fin, "%d", &id);
            show_unit(sys, fout, id);
        } else if (strcmp(op, "SHOW_INCIDENT") == 0) {
            int id; 
            fscanf(fin, "%d", &id);
            show_incident(sys, fout, id);
        } else if (strcmp(op, "SHOW_INTERVENTIONS") == 0) {
            show_interventions(sys, fout);
        }
    }
    
    // Free the used memory
    free_system(sys);
    fclose(fin);
    fclose(fout);

    return 0;
}
