#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    // Open syslog with LOG_USER facility
    openlog("writefile", LOG_PID, LOG_USER);

    // Check for correct number of arguments
    if (argc != 3) {
        syslog(LOG_ERR, "Incorrect number of arguments. Usage: ./writefile <file_path> <text_string>");
        fprintf(stderr, "Error: Incorrect number of arguments. Usage: ./writefile <file_path> <text_string>\n");
        closelog();
        exit(1);
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    // Try to open the file for writing
    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Could not create file %s: %s", writefile, strerror(errno));
        fprintf(stderr, "Error: Could not create file %s: %s\n", writefile, strerror(errno));
        closelog();
        exit(1);
    }

    // Write the string to the file
    if (fprintf(file, "%s", writestr) < 0) {
        syslog(LOG_ERR, "Failed to write to file %s: %s", writefile, strerror(errno));
        fprintf(stderr, "Error: Failed to write to file %s: %s\n", writefile, strerror(errno));
        fclose(file);
        closelog();
        exit(1);
    }

    // Log the successful write operation
    syslog(LOG_DEBUG, "Writing '%s' to '%s'", writestr, writefile);

    // Clean up
    fclose(file);
    closelog();

    return 0;
}

