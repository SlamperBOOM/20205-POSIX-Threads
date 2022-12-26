#ifndef UTILITIES_SYNC_PIPE_H
#define UTILITIES_SYNC_PIPE_H

int sync_pipe_init();
void sync_pipe_close();
int sync_pipe_wait();
void sync_pipe_notify(int num_really_created_threads);
int get_rfd_spipe();

#endif //UTILITIES_SYNC_PIPE_H
