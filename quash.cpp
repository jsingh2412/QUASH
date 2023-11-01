#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem>
#include <regex>
#include <fcntl.h>

void executeCommand(char **argv, bool runInBackground);
void executeEcho(const std::vector<std::string> &args, std::string input);
void ls(const std::vector<std::string> args);
void cd(const std::vector<std::string> &args);
void cat(const std::vector<std::string> &args);
void pwd();
void setEnv(const std::vector<std::string> args);
void setDefaultPath();
void replaceEnv(const std::vector<std::string> args, std::string &input);
void removeComments(std::string &input);
void pipeCommands(std::vector<std::string> commands);
void removeQuotes(std::string &str);
void handlePassing(std::string input, bool runInBackground);
void freeCommands(std::vector<char**>& vecCmds);

struct Job
{
    int jobID;
    pid_t pid;
    std::string command;
};

std::vector<Job> jobs;
int jobCounter = 1;

int main()
{
    setDefaultPath();
    

    while (true)
    {
        int status;
        for (auto it = jobs.begin(); it != jobs.end();)
        {
            pid_t result = waitpid(it->pid, &status, WNOHANG);
            if (result == it->pid)
            {
                // Job has completed
                it = jobs.erase(it);
            }
            else
            {
                ++it;
            }
        }


        std::cout << "[QUASH]$ ";

        std::string input;
        getline(std::cin, input);
        std::vector<std::string> args;
        std::string arg;

        removeComments(input);
        

        std::istringstream iss(input);
        while (iss >> arg)
        {
            args.push_back(arg);
        }

        // anytime no input is given
        if (args.size() <= 0)
        {
            continue;
        }
        
        if (args[0] == "exit" || args[0] == "quit")
        {
            break;
        }
        // safe even if string is less than 6 chars
        if (input.compare(0, 6, "export") != 0)
        {
            replaceEnv(args, input);
        }
        if (args[0] == "echo")
        {
            executeEcho(args, input);
        }
        else if (args[0] == "cd")
        {
            cd(args);
        }
        else if (args[0] == "pwd")
        {
            pwd();
        }
        else if (args[0] == "export")
        {
            setEnv(args);
        }
        else if (args[0] == "jobs")
        {
            std::cout<<"Current Jobs:\n";
            for (const Job &job : jobs)
            {
                std::cout << "[" << job.jobID << "] " << job.pid << " " << job.command << std::endl;
            }
            //} else if(args[0] == "cat"){
            //    cat(args);
        }
        else if (args[0] == "kill")
        {
            if (args.size() != 3)
            {
                std::cerr << "Usage: kill SIGNUM PID" << std::endl;
            }
            else
            {
                int signum = std::stoi(args[1]);
                pid_t targetPID = std::stoi(args[2]);
                if (kill(targetPID, signum) != 0)
                {
                    std::cerr << "kill failed for PID " << targetPID << " with signal " << signum << std::endl;
                }
            }
        }
        else
        {
            bool runInBackground = false;
            if (!args.empty() && args.back() == "&")
            {
                runInBackground = true;
                args.pop_back(); // Remove the "&" from the arguments
            }
            handlePassing(input, runInBackground);
        }
    }

    return 0;
}

void executeCommand(char **argv, bool runInBackground)
{
    int orgStdin = dup(STDIN_FILENO);
    int orgStdout = dup(STDOUT_FILENO);
    pid_t pid, wpid;
    int status;
    pid = fork();

    int redirectIndex = 0;
    bool outputRedir = false;
    bool inputRedir = false;
    bool appendRedir = false;
    int fd;
    
    for(int i=0; argv[i] != NULL; i++) {
        //cat b.txt > a.txt
        if(strcmp(argv[i], ">") == 0) {
            redirectIndex = i;
            fd = open(argv[i+1], O_WRONLY|O_TRUNC|O_CREAT, 0666); 
            dup2(fd, STDOUT_FILENO);
            close(fd);
            outputRedir = true; 
        }
        //grep -o World a.txt >> b.txt
        else if(strcmp(argv[i], ">>") == 0) {
            fd = open(argv[i+1], O_WRONLY|O_APPEND|O_CREAT, 0666);
            dup2(fd, STDOUT_FILENO);
            close(fd);
            appendRedir = true;
        }
        //cat < a.txt 
        else if(strcmp(argv[i], "<") == 0) {
            redirectIndex = i;
            fd = open(argv[i+1], O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);
            inputRedir = true;
        }
    }

    int argc = 0;
    while(argv[argc] != NULL) {
        argc++; 
    }
    //if argc is ever needed past this update in if statements
    if (inputRedir) {
        for(int j=redirectIndex; j<argc-1; j++) {
            argv[j] = argv[j+1]; 
        }
        argv[argc-1] = NULL;
    } else if(outputRedir) {
        for (int i = redirectIndex; i < argc - 2; i++) {
            argv[i] = argv[i + 2];
        }
        argv[argc - 2] = NULL;
        argv[argc - 1] = NULL;
    } else if(appendRedir){
        for(int j=redirectIndex; j<argc-1; j++) {
            argv[j] = argv[j+1]; 
        }
        argv[argc-1] = NULL;
    }
    // child process
    if (pid == 0)
    {
        if (!inputRedir) {
            close(orgStdout);
            dup2(orgStdin, STDIN_FILENO);
            close(orgStdin);
        }
        if (!outputRedir || !appendRedir) {
            close(orgStdin);
            dup2(orgStdout, STDOUT_FILENO);
            close(orgStdout);
        }
        close(orgStdin);
        close(orgStdout);
        if (execvp(argv[0], argv) == -1)
        {
            std::cerr << ("Unknown command.\n");
        }
        exit(EXIT_FAILURE);
    }
    else
    {   //parent process
        //restore STDIN and STDOUT
        dup2(orgStdin, STDIN_FILENO);
        dup2(orgStdout, STDOUT_FILENO);

        close(orgStdin);
        close(orgStdout);

        if (runInBackground)
        {
            // If the command is run in the background, add it to the list of jobs
            Job job;
            job.jobID = jobCounter++;
            job.pid = pid;
            job.command = argv[0];
            jobs.push_back(job);
        }
        else
        {
            // If the command is not run in the background, wait for it to finish
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

void executeEcho(const std::vector<std::string> &args, std::string input)
{
    std::string breakArg = "";
    bool fullEcho = true;
    std::string tempStr = input;
    // check all args to see if anything else exists beside echo within the input
    if (args.size() < 2)
    {
        std::cerr << "Error: Please pass arguments to echo." << std::endl;
    }
    else
    {
        removeQuotes(input);

        for (auto arg : args)
        {
            if (arg == "|")
            {
                // figure out a way to return the position of this arg so we can still echo everything before
                fullEcho = false;
                break;
            }
        }
        if (fullEcho)
        {
            // works for most basic of strings
            // removes "echo "
            std::string msg = input.substr(5) + "\n";
            std::cout << msg;
            // should I be ignoring all occurences of " or only if there is a second one?
        }
    }
}

//NOT USED
void ls(const std::vector<std::string> args)
{
    // std::vector<std::string> entries;
    for (const auto &entry : std::filesystem::directory_iterator("."))
    {
        // entries.push_back(entry.path().filename().string());
        std::cout << entry.path().filename().string() << std::endl;
    }
}

void cd(const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        std::cerr << "Usage: cd <directory>" << std::endl;
    }
    else
    {
        // do i need to be able to go into the directory EECS 678 using cd "EECS 678"
        if (chdir(args[1].c_str()) != 0)
        {
            std::cerr << "cd failed: " << args[1] << std::endl;
        }
    }
}

void pwd()
{
    char *cwd = get_current_dir_name();
    if (cwd != nullptr)
    {
        std::cout << "Current working directory: " << cwd << std::endl;
        free(cwd);
    }
    else
    {
        std::cerr << "Unable to retrieve current working directory." << std::endl;
    }
}

// export $ENVVAR=SOMEPATH
void setEnv(const std::vector<std::string> args)
{
    if (args.size() < 2)
    {
        std::cerr << "Usage: export $ENVVAR=SOMEPATH" << std::endl;
    }
    else
    {
        std::string str = args[1];
        size_t equalPos = str.find('=');
        // if '=' is found
        if (equalPos != std::string::npos)
        {
            std::string name = str.substr(1, equalPos-1);
            std::string value = str.substr(equalPos + 1);
            // returns 0 when good, -1 when bad
            int result = setenv(name.c_str(), value.c_str(), 1);
            if (result == 0)
            {
                std::cout << "Environment variable set: " << name << " = " << value << std::endl;
            }
            else
            {
                std::cerr << "Failed to set environment variable." << std::endl;
            }
        }
        else
        {
            std::cerr << "Usage: export $ENVVAR=SOMEPATH" << std::endl;
        }
    }
}

void setDefaultPath()
{
    setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
}

void removeComments(std::string &input)
{
    size_t commentPos = input.find('#');
    if (commentPos != std::string::npos)
    {
        input = input.substr(0, commentPos - 1);
    }
}

void replaceEnv(const std::vector<std::string> args, std::string &input)
{
    size_t envPos = input.find('$');
    if (envPos != std::string::npos)
    {
        
        for (auto arg : args)
        {
            if (arg[0] == '$')
            {
                std::string tempStr = "";
                tempStr = arg.substr(1, arg.length() - 1);
                size_t len = tempStr.length() - 1;
                if (getenv(tempStr.c_str()) == NULL)
                {
                    // std::cerr << "No environment variable has been declared with the name " << tempStr << ".\n";
                    arg = "";
                    input.replace(envPos, envPos + len, arg);
                }
                else
                {
                    arg = getenv(tempStr.c_str());
                    input.replace(envPos, envPos + len, arg);
                }
            }
        }
    }
}
//TEST
//cat a.txt | grep -o World
void pipeCommands(std::vector<std::string> commands)
{   
    int status;
    char **cmd1;
    char **cmd2;
    char **cmd3;

    bool thirdChild = (commands.size() >= 3);

    std::vector<char**> parsedCommands;
  
    for(const std::string& command : commands){
        std::vector<char*> args;

        std::istringstream iss(command);
        std::string arg;
        
        while(iss >> arg){
        args.push_back(strdup(arg.c_str()));  
        }

        args.push_back(nullptr); 

        char** argv = new char*[args.size()];
        for(long unsigned int i = 0; i < args.size(); i++){
            argv[i] = args[i]; 
        }

        parsedCommands.push_back(argv);
    }
    size_t j = 0;
    for(auto& argv : parsedCommands){
        if(j == 0){
            cmd1 = parsedCommands.at(j);
        }
        else if(j == 1){
            cmd2 = parsedCommands.at(j); 
        }
        else if(j == 2){
            cmd3 = parsedCommands.at(j);
        }
        j++;
    }

    int p1[2], p2[2];
    if (pipe(p1) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    if (pipe(p2) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid1, pid2, pid3;

    // Create the first child process
    if ((pid1 = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
   
    //child 1
    if (pid1 == 0) {
        //close unused pipes
        close(p2[0]);
        close(p2[1]);
        close(p1[0]);
        dup2(p1[1], STDOUT_FILENO); 
        close(p1[1]);
        
        //for (int i = 0; cmd1[i] != nullptr; i++) {
        //    std::cout << cmd1[i] << " ";
        //}
        //std::cout << std::endl;
        execvp(cmd1[0], cmd1);
        perror("execvp");
        exit(EXIT_FAILURE); 
    }

    //create second child process
    if ((pid2 = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid2 == 0) {
        //child 2
        if(!thirdChild){
            close(p2[1]);
        }
        close(p2[0]);
        close(p1[1]);        
        dup2(p1[0], STDIN_FILENO);
        close(p1[0]); 

        if(thirdChild) {
            dup2(p2[1], STDOUT_FILENO);
            close(p2[1]); 
        }
        
        //for (int i = 0; cmd2[i] != nullptr; i++) {
        //    std::cout << cmd2[i] << " ";
        //}
        //std::cout << std::endl;
        execvp(cmd2[0], cmd2);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    if(thirdChild){
        if ((pid3 = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid3 == 0) {
            //child 3
            close(p1[0]);
            close(p1[1]);
            close(p2[1]);
            dup2(p2[0], STDIN_FILENO); 
            close(p2[0]);

            execvp(cmd3[0], cmd3); 
        }
    }
    //wait for the child processes to finish
    //close pipe ends in the parent process
    close(p1[0]);
    close(p1[1]);
    close(p2[0]);
    close(p2[1]); 
    waitpid(pid1, &status, 0);
    //std::cout<<"CHILDONEEXITED\n";
    waitpid(pid2, &status, 0);
    //std::cout<<"CHILDTWOEXITED\n";
    if (thirdChild) {
        waitpid(pid3, &status, 0);
    }
    //std::cout<<"WHATWHATEXITED\n";
    //close pipe ends in the parent process
    freeCommands(parsedCommands);
}

// echo helper function
void removeQuotes(std::string &input)
{
    size_t quote_start = std::string::npos;

    for (size_t i = 0; i < input.length(); i++)
    {
        if ((input[i] == '"') && (quote_start == std::string::npos))
        {
            quote_start = i;
        }
        else if ((input[i] == '"') && (quote_start != std::string::npos))
        {
            input.erase(i, 1);
            input.erase(quote_start, 1);

            quote_start = std::string::npos;
        }
    }
}

void handlePassing(std::string input, bool runInBackground)
{
    // Tokenize the input into individual commands based on '|'
    int inFd, outFd;
    int stdOut = dup(STDOUT_FILENO), stdIn = dup(STDIN_FILENO);
    bool prevWasSTDIN = false, prevWasSTDOUT = false, prevWasAPPEND = false;
    std::vector<std::string> commands;
    std::istringstream ss(input);
    std::string command;
    int numOfCommands = 0;

    while (std::getline(ss, command, '|'))
    {
        commands.push_back(command);
        numOfCommands++;
    }
    
    if(numOfCommands>1){
        pipeCommands(commands);
        return;
    }

    // Execute each command
    for (const std::string &cmd : commands)
    {
        // Tokenize the individual command into arguments
        std::istringstream argStream(cmd);
        std::string arg;
        std::vector<char *> args;
        
        while (argStream >> arg)
        {
            args.push_back(strdup(arg.c_str()));
        }
        args.push_back(nullptr);
        
        // Execute the command
        executeCommand(args.data(), runInBackground);

        // Clean up allocated memory
        for (char *argPtr : args)
        {
            free(argPtr);
        }
    }
}

void freeCommands(std::vector<char**>& vecCmds) {
    for (char** charArray : vecCmds) {
        for (int i = 0; charArray[i] != nullptr; i++) {
            free(charArray[i]); // Deallocate each char* in the char**
        }
        delete[] charArray; // Deallocate the char** itself
    }
    vecCmds.clear();
}


// NOT USED
void cat(const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        std::cerr << "Usage: cat <filename>\n";
        return;
    }

    FILE *fp = fopen(args[1].c_str(), "r");

    if (fp == NULL)
    {
        std::cerr << "Error opening file.\n";
        return;
    }
    char ch;
    while ((ch = fgetc(fp)) != EOF)
    {
        putchar(ch);
    }
    std::cout << std::endl;

    fclose(fp);
}