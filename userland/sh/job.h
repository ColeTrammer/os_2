#ifndef _JOB_H
#define _JOB_H 1

#include <sys/types.h>

#define MAX_JOBS 20

enum job_state { DNE = 0, RUNNING, STOPPED, TERMINATED };

#define JOB_MAX_PIDS 10

struct job {
    enum job_state state;
    int pgid;
    int num_processes;
    int num_consumed;
    pid_t pids[10];
};

enum job_id_type { JOB_PGID, JOB_ID };

struct job_id {
    enum job_id_type type;
    union {
        pid_t pgid;
        pid_t jid;
    } id;
};

pid_t get_pgid_from_jid(pid_t jid);
pid_t get_pgid_from_pid(pid_t pid);
pid_t get_pgid_from_id(struct job_id id);
pid_t get_jid_from_pgid(pid_t pgid);
void print_job(pid_t jid);

void job_print_all();
void job_add(pid_t pgid, pid_t pids[], int num_prcessed, enum job_state state);

struct job_id job_id(enum job_id_type type, pid_t id);
int job_run(struct job_id id);
int job_run_background(struct job_id id);

void job_check_updates(bool print_updates);

#endif /* _JOB_H */
