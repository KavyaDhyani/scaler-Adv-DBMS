#include <unistd.h>
#include <fcntl.h>

int main() {

    // Open file for both reading and writing
    int fd = open(
        "sample.txt",
        O_RDWR | O_CREAT | O_APPEND,
        0644
    );

    if (fd < 0) {
        const char *msg = "File open failed\n";
        write(1, msg, 17);
        return 1;
    }

    // Buffer for reading
    char buffer[128];

    // Read existing file contents
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));

    // Display existing contents on terminal
    if (bytesRead > 0) {

        const char *msg = "Existing File Content:\n";
        write(1, msg, 23);

        write(1, buffer, bytesRead);
    }

    // New data to append into file
    const char *newData =
        "\nNew line written using system calls.\n";

    // Write new data into file
    write(fd, newData, 39);

    // Close file
    close(fd);

    return 0;
}