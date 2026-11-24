#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

int  pm_init(void);
int  pm_start_all(void);
void pm_handle_children(void);
void pm_periodic_check(void);
void pm_report_status(void);
void pm_stop_restarting(void);
void pm_terminate_all(void);
void pm_kill_remaining(void);
int  pm_alive_count(void);

#endif