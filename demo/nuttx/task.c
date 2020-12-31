#include <nng/nuttx/nxipc.h>
#include <nng/nuttx/nxtask.h>
#include <sys/syscall.h>
#define gettidv1() syscall(__NR_gettid)

static void delayed_work(void* arg) {
    int* interrupted = (int*)arg;
    *interrupted = 1;
    printf("delayed_work.tid=%d\n", gettidv1());
}

int main(int argc, char* argv[]) {
    int interrupted = 0;
    int cnt = 0;

    const void* server = nxipc_server_create("ipctask");

    printf("test nxtask_delay!\n");
    while(cnt++ < 10) {
        interrupted = 0;
        nxtask_delay(1000, delayed_work, &interrupted);
        while (interrupted == 0) {
            usleep(10000);
        }
    }
    printf("test nxtask_async!\n");
    nxtask_async(delayed_work, &interrupted);
    nxtask_async(delayed_work, &interrupted);
    nxtask_async(delayed_work, &interrupted);

    nxipc_server_release(server);
    return 0;
}
