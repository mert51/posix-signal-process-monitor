#ifndef SIGNAL_MANAGER_H
#define SIGNAL_MANAGER_H

#include <signal.h>

/* Handler'lar sadece bu bayrakları set eder; asıl işi main loop yapar. */
extern volatile sig_atomic_t g_shutdown_requested; /* SIGINT, SIGTERM */
extern volatile sig_atomic_t g_report_requested;   /* SIGUSR1 */
extern volatile sig_atomic_t g_sigchld_received;   /* SIGCHLD */
extern volatile sig_atomic_t g_alarm_fired;        /* SIGALRM */

/* Tüm sigaction kurulumlarını yapar. Başarıda 0, hatada -1. */
int signal_manager_init(void);

/* Yönetilen sinyalleri bloklar, önceki maskeyi *old'a yazar. */
int signal_manager_block(sigset_t *old);

/* Maskeyi old'a geri döndürür. fork() sonrası çocukta da kullanılır. */
int signal_manager_restore(const sigset_t *old);

#endif
