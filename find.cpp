#include <iostream>
#include <vector>
#include <queue>

#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <wait.h>
#include <dirent.h>


void printErr(const std::string& message) {
    fprintf(stderr, "ERROR %s: %s\n", message.c_str(), strerror(errno));
}

enum sizeArea {
    EMPTY, LESS, GREATER, EQUAL
};

struct Request {
    std::string filePath;

    bool inumNeeded = false,
            nameNeeded = false,
            sizeNeeded = false,
            nlinksNeeded = false,
            execNeeded = false;

    ino_t inodeNumber;
    std::string name, executionPath;
    nlink_t nlinkNumber;
    off_t size;
    sizeArea neededSizeArea = EMPTY;

};

struct BfsNode {
    DIR* dir;
    std::string path;
};

int parseNumber(const std::string& number) {
    int result = 0;

    try {
        result = std::stoll(number);
    } catch (const std::exception& e) {
        printErr(e.what());
        exit(EXIT_FAILURE);
    }

    return result;
}

void correctPath(std::string& path) {

    if (path.back() != '/') {
        path.append("/");
    }

}

void parseArgs(Request& request, int argc, char* argv[]) {

    for (int i = 2; i < argc; i += 2) {
        std::string argument(argv[i]);
        std::string value(argv[i + 1]);

        if (argument == "-inum" || argument == "-nlinks") {
            int numericValue = parseNumber(value);

            if (argument == "-inum") {
                request.inodeNumber = numericValue;
            } else {
                request.nlinkNumber = numericValue;
            }

        } else if (argument == "-name") {
            request.nameNeeded = true;
            request.name = value;

        } else if (argument == "-path") {
            request.nameNeeded = true;
            request.name = value;

        } else if (argument == "-size") {

            if (value[0] == '-') {
                request.neededSizeArea = LESS;
            } else if (value[0] == '=') {
                request.neededSizeArea = EQUAL;
            } else if (value[0] == '+') {
                request.neededSizeArea = GREATER;
            } else {
                printErr("Invalid value for -size argument");

                exit(EXIT_FAILURE);
            }

            request.size = parseNumber(value.substr(1));

        } else if (argument == "-exec") {
            request.execNeeded = true;
            request.executionPath = value;
        }
    }

    request.filePath = argv[1];
}

void execute(const std::string& command, const std::vector<std::string>& args) {
    pid_t pid = fork();

    if (pid == -1) {
        printErr("Unable to create child process");

        exit(EXIT_FAILURE);

    } else if (pid == 0) {
        char** charArgs = new char* [args.size() + 2];
        char* envp[] = {nullptr};

        charArgs[0] = const_cast<char*>(command.c_str());

        for (size_t i = 1; i <= args.size(); ++i) {
            charArgs[i] = const_cast<char*>(args[i - 1].c_str());
        }

        charArgs[args.size() + 1] = nullptr;

        if (execve(charArgs[0], charArgs, envp) == -1) {
            printErr("Execution failed");

            exit(EXIT_FAILURE);
        }

    } else {
        int waitingStatus;

        if (waitpid(pid, &waitingStatus, 0) == -1) {
            printErr("Error while waiting");
        } else {
            printf("Process finished with exit code %d\n", WEXITSTATUS(waitingStatus));
        }

    }
}

bool checkStat(const Request& request, const struct stat& stat, const std::string& name) {

    if (request.inumNeeded) {
        if (stat.st_ino != request.inodeNumber) {
            return false;
        }
    }

    if (request.nlinksNeeded) {
        if (stat.st_nlink != request.nlinkNumber) {
            return false;
        }
    }

    if (request.nameNeeded) {
        if (name != request.name) {
            return false;
        }
    }

    if (request.sizeNeeded) {

        if (request.neededSizeArea == LESS) {

            if (stat.st_size >= request.size) {
                return false;
            }

        } else if (request.neededSizeArea == EQUAL) {

            if (stat.st_size != request.size) {
                return false;
            }

        } else if (request.neededSizeArea == GREATER) {

            if (stat.st_size <= request.size) {
                return false;
            }

        }
    }

    return true;
}

std::vector<std::string> bfs(const Request& request) {
    std::vector<std::string> result;
    std::queue<BfsNode> q;

    DIR* root = opendir(request.filePath.c_str());
    q.push({root, request.filePath});

    if (root != nullptr) {
        while (!q.empty()) {
            BfsNode current = q.front();
            q.pop();

            while (dirent* file = readdir(current.dir)) {
                struct stat sb{};

                char* fileName = file->d_name;
                std::string filePath = current.path;
                correctPath(filePath);

                if (fileName != nullptr) {

                    if (!(strcmp(fileName, ".") == 0 || strcmp(fileName, "..") == 0)) {
                        std::string fullPath = filePath + fileName;
                        std::string name(fileName);

                        if (lstat(fullPath.c_str(), &sb) != 1) {

                            if (S_ISDIR(sb.st_mode)) {
                                DIR* nextDirectory = opendir(fullPath.c_str());

                                if (!nextDirectory) {
                                    q.push({nextDirectory, fullPath});
                                }
                            } else if (checkStat(request, sb, name)) {
                                result.push_back(fullPath);
                            }

                        } else {
                            printErr("Unable to access file");
                        }

                    }
                }
            }

            closedir(current.dir);
        }

    } else {
        printErr("Unable to access root directory");
    }

    return result;
}


int main(int argc, char* argv[]) {
    //Simple test: /home/dmitriy/CLionProjects/os-kidshell/ -name shell.cpp -exec /usr/bin/sha1sum
    //Press F for environment
    Request request;

    if (argc < 2 || ((argc - 2) % 2 == 1)) {
        printErr("Invalid number of arguments");

        exit(EXIT_FAILURE);
    }

    parseArgs(request, argc, argv);

    std::vector<std::string> files = bfs(request);

    if (request.execNeeded) {
        execute(request.executionPath, files);

    } else {
        for (std::string& file: files) {
            printf("%s\n", file.data());
        }

    }

    exit(EXIT_SUCCESS);
}