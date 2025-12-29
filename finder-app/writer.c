/**
 * @file writer.c
 * @author Samuel S
 * 
 */
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>

int main(int argc, const char *argv[])
{
    openlog(NULL, 0, LOG_USER);
    if (argc != 3) {
        syslog(LOG_USER | LOG_ERR, "Expected 2 arguments but received %d\n", argc-1);
        return 1;
    }
    const char *writefile = argv[1];
    const char *writestr = argv[2];

    int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (-1 == fd) {
        syslog(LOG_USER | LOG_ERR, "Failed to create path: %s\n", writefile);
        return 1;
    } else {
        syslog(LOG_USER | LOG_DEBUG, "Writing %s to %s", writestr, writefile);
        int nwrite = write(fd, writestr, strlen(writestr));
        if (-1 == nwrite) {
            syslog(LOG_USER | LOG_ERR, "Failed to write to path: %s\n", writefile);
            return 1;
        }
        int close_state = close(fd);
        if (-1 == close_state) {
            syslog(LOG_USER | LOG_ERR, "An error occurred whiles closing file '%s'\n", writefile);
            return 1;
        }
    }
    closelog();
    return 0;
}
