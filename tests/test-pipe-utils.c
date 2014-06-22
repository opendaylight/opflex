#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>

#define DBUG_OFF 1 //turn off debugging

#include "ovs-thread.h"
#include "misc-util.h"
#include "dbug.h"

/* protos */
static void write_pipe(void **state);

static void write_pipe(void **state) {
    (void) state;
    FILE *pipe_p;
    FILE *file_p;
    char str_to_check[6];
    char *str;

    DBUG_PUSH("d:t:i:L:n:P:T:0");
    DBUG_ENTER("write_pipe");

    pipe_p = pipe_write("echo Hello > /tmp/pipe_write_test");
    sleep(1);

    file_p = fopen("/tmp/pipe_write_test","r");
    //printf("file_p is %p\n",file_p);
    assert_non_null(file_p);

    str = fgets(&str_to_check,6,file_p);
    assert_non_null(str);

    assert_int_equal(strncmp(&str_to_check,"Hello",5),0);
    assert_int_equal(fclose(file_p),0);
    assert_int_equal(pipe_close(pipe_p),0);

    assert_int_equal(remove("/tmp/pipe_write_test"),0);

    DBUG_LEAVE;
}

int main(void) {
    const UnitTest tests[] = {
        unit_test(write_pipe),
    };
    return run_tests(tests);
}
