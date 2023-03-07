#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include <vector>
#include <string>
#include <cstring>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    //create copies of std/stdout using dup()
    //int origOut = dup(0);
    //int origIn = dup(1);
    string prev_dir; //store prev dir
    vector<pid_t> bg_pids;
    for (;;) {
        int origOut = dup(0); // create a copy of stdin
        int origIn = dup(1); // create a copy of stdout //fix restore ones to see
        //TODO: implement iteration over vector of bg pid (vector also declared outside loop)
        //TODO: waitpid() - using flag for non-blocking
        for (auto i = bg_pids.begin(); i != bg_pids.end(); ) {
            int status;

            pid_t result = waitpid(*i, &status, WNOHANG);
            if (result == -1) {
                // error occurred while checking process status
                cerr << "Error while waiting for background process: " << strerror(errno) << endl;
                i++;
            } 
            else if (result == 0) {
                // process is still running
                i++;
            } 
            else {
                i = bg_pids.erase(i);
            }
        }

        
       
        time_t current_time;
        time(&current_time);

        char* cwd = getcwd(NULL, 0);
        cout << YELLOW << ctime(&current_time) << getenv("USER") << ":" << cwd << "$" << NC << " ";
        free(cwd);
        
        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        //TODO: chdir()
        //if dir (cd <dir>) is "-", then got to prev working directory, if not call chdir()w requred parameters
        //variable storing previous working directory (it needs to be declared outside loop)
    
        // get tknrized commands from user input
        Tokenizer tknr(input);
            if (tknr.commands[0]->args.size() > 0 && tknr.commands[0]->args[0] == "cd") {
                string dir;
                if (tknr.commands[0]->args.size() > 1) {
                    dir = tknr.commands[0]->args[1];
                } 
                else {
                    dir = getenv("HOME");  // change to home directory if no argument provided
                }

                if (dir == "-") {  // change to previous directory
                    if (prev_dir.empty()) {
                        cerr << "cd -" << endl;
                    } 
                    else {
                        dir = prev_dir;
                    }
                }
                
                    // save current directory as previous
                    char* copy = getcwd(NULL, 0);  // get current directory to set as previous
                    prev_dir = copy;
                    free(copy);  // free the memory allocated by getcwd()
                   
                
                

                if (chdir(dir.c_str()) != 0) {  // change directory and check for errors
                    cerr << "Shell: cd: " << dir << ": " << strerror(errno) << endl;
                }
                continue;
            }
        
        
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }


        // // print out every command tknr-by-tknr on individual lines
        // // prints to cerr to avoid influencing autograder
         for (auto cmd : tknr.commands) {
             for (auto str : cmd->args) {
                 cerr << "|" << str << "| ";
             }
             if (cmd->hasInput()) {
                 cerr << "in< " << cmd->in_file << " ";
             }
             if (cmd->hasOutput()) {
                 cerr << "out> " << cmd->out_file << " ";
             }
             cerr << endl;
         }

        //for piping
        // for cmd : commands
        //      call pipe() to make pipe
        //      fork() - in child, redirect stdout; in paretn redirect stdin
        //       ^is already written
        //     TODO:  add checks for first/last command (first: wouldnt redirect the stdin in the parent. last: wouldnt redirect stdout of the child )
        // Create pipe

        for(long unsigned int i = 0; i < tknr.commands.size(); i++){
           int p[2];
            if (pipe(p) < 0){
                cout << "ERROR: Failed to open pipe\n" << endl;
                return 0;
            }
                
            // Create child to run first command
            int pid = fork();

            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            //TODO: add check for bg process - add pid to vector if bg and don't waitpid() in parent

            // Handle background processes
             if (pid == 0) {
                close(p[0]);
                
                bool bg = tknr.commands[i]->isBackground();
                if (bg) {
                    // Redirect standard input 
                    if (!tknr.commands[i]->hasInput()) {
                        freopen("/dev/null", "r", stdin);
                    }

                    // Redirect standard output
                    if (!tknr.commands[i]->hasOutput()) {
                        freopen("/dev/null", "w", stdout);
                    }
                    
                }
            
            

                // Redirect input from file
                if (tknr.commands[i]->hasInput()) {
                    string infile = tknr.commands[i]->in_file;
                    int fd = open(infile.c_str(), O_RDONLY);
                    if (fd < 0) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                // Redirect output to file
                if (tknr.commands[i]->hasOutput()) {
                    string outfile = tknr.commands[i]->out_file;
                    int fd = open(outfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                    if (fd < 0) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                // Redirect standard input/output to pipes if not first/last command
                /*if (i > 0) {
                    dup2(p[0], STDIN_FILENO);
                    close(p[0]);
                }
                */
                if (i < tknr.commands.size()-1){
                    close(p[0]);
                    dup2(p[1], STDOUT_FILENO);
                }

                

                // access the arguments for the command
                vector<string>& args = tknr.commands[i]->args;

                // convert the vector of arguments to a char** array
                char** temp = new char*[args.size() + 1];
                for (long unsigned int j = 0; j < args.size(); j++) {
                    temp[j] = new char[args[j].length() + 1];
                    strcpy(temp[j], args[j].c_str());
                }
                temp[args.size()] = NULL;

                // execute the command
                execvp(temp[0],temp); 
                perror("execvp");
                for (long unsigned int j = 0; j < args.size(); j++) {
                    delete[] temp[j];
                }
                delete[] temp;
                exit(EXIT_FAILURE);
            }

            else {  // if parent, wait for child to finish
                bool bg = tknr.commands[i]->isBackground();
                if (bg) {
                    bg_pids.push_back(pid);
                    // continue without waiting for child
                    continue;
                }
                
                int status = 0;
                if(i == tknr.commands.size()-1){
                    waitpid(pid, &status, 0);
                    if (status > 1) {  // exit if child didn't exec properly
                        exit(status);
                    }
                }
                if (tknr.commands.size() > 1) { 
                    dup2(p[0], STDIN_FILENO);
                    close(p[1]);
                }
            
                
            }
          
                    

        }  
        //restore std in/out
        dup2(origOut, 0); 
        dup2(origIn, 1); 
        
          
    }

}