#ifndef __COMPUTATION_H__
#define __COMPUTATION_H__
#endif

#include <stdbool.h>
#include "messages.h"

void computation_init(void);
void computation_cleanup(void);

bool is_computing(void);
bool is_done(void);

void abort_comp(void);

bool set_compute(message *msg);
bool compute(message *msg);

void update_data(const msg_compute_data *compute_data);

