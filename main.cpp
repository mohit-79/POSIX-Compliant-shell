#include <iostream>
#include <limits.h> //PATH_MAX
#include <cstdlib> //getenv,exit
#include <sstream> //stringstream
#include <unistd.h> //access(),fork(),exec*,chdir() checks if file exists or not 
#include <string>
#include <errno.h> //perror()
#include <vector>
#include <set>
#include <cstring> //strerror()
#include <sys/wait.h>
#include <filesystem>
#include <fcntl.h>   // open(), O_CREAT, O_TRUNC
#include <readline/readline.h>
#include <readline/history.h>
#include <cctype>
#include <dirent.h>   // opendir, readdir
#include <sys/stat.h> // stat
#include <readline/history.h>
#include <fstream>

std::set<std::string> builtin_command={
    "exit",
    "echo",
    "type",
    "pwd",
    "history"
  };


struct redirection{
  bool redirect_stdout=false;
  bool redirect_stderr=false;
  bool append_stdout=false;
  bool append_stderr=false;

  std::string stdout_file;
  std::string stderr_file;
};

static int last_written_history=0;
int saved_stdout = -1;
int saved_stderr = -1;
redirection parse_redirection(std::vector<std::string>&args){
  redirection r;
  for(size_t i=0;i<args.size();){
    if(args[i]==">" || args[i]=="1>"){
      r.redirect_stdout=true;
      r.append_stdout=false;
      r.stdout_file=args[i+1];
      args.erase(args.begin()+i,args.begin()+i+2);
    }
    else if(args[i]==">>" || args[i]=="1>>"){
      r.redirect_stdout=true;
      r.append_stdout=true;
      r.stdout_file=args[i+1];
      args.erase(args.begin()+i,args.begin()+i+2);
    }
    else if(args[i]=="2>"){
      r.redirect_stderr=true;
      r.append_stderr=false;
      r.stderr_file=args[i+1];
      args.erase(args.begin()+i,args.begin()+i+2);
    }
    else if(args[i]=="2>>"){
      r.redirect_stderr=true;
      r.append_stderr=true;
      r.stderr_file=args[i+1];
      args.erase(args.begin()+i,args.begin()+i+2);
    }
    else i++;
  }
  return r;
}
void apply_redirections(const redirection& r){
  if(r.redirect_stdout){
    saved_stdout = dup(STDOUT_FILENO);
    int flags=O_WRONLY | O_CREAT | (r.append_stdout?O_APPEND:O_TRUNC);
    int fd=open(r.stdout_file.c_str(),flags,0644);
    if(fd<0){
      perror("open stdout");
      exit(1);
    }
    dup2(fd,STDOUT_FILENO);
    close(fd);
  }
  if(r.redirect_stderr){
    saved_stderr = dup(STDERR_FILENO);
    int flags=O_WRONLY | O_CREAT | (r.append_stderr?O_APPEND:O_TRUNC);
    int fd=open(r.stderr_file.c_str(),flags,0644);
    if(fd<0){
      perror("open stderr");
      exit(1);
    }
    dup2(fd,STDERR_FILENO);
    close(fd);
  }
}
void restore_builtin_redirection(){
    if(saved_stdout != -1){
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        saved_stdout = -1;
    }
    if(saved_stderr != -1){
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
        saved_stderr = -1;
    }
}

std::vector<std::string> parse_command(const std::string& command) {
    std::vector<std::string>args;
    std::string current;

    enum State{NORMAL,SINGLE,DOUBLE };
    State state=NORMAL;

    for(size_t i=0;i<command.size();i++) {
        char c=command[i];

        if(state==NORMAL) {
            if(isspace(c)){
                if(!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            }
            else if(c=='\'') {
                state=SINGLE;
            }
            else if(c=='"') {
                state=DOUBLE;
            }
            else if(c=='\\') {
                if (i+1<command.size())
                    current+=command[++i];
                else{
                    std::cerr<<"syntax error: trailing backslash\n";
                    return {};
                }
            }
            else{
                current+=c;
            }
        }

        else if(state==SINGLE) {
            if(c=='\'')
                state=NORMAL;
            else
                current+=c;
        }

        else if(state==DOUBLE) {
            if(c=='"'){
                state=NORMAL;
            }
            else if(c=='\\') {
                if(i+1<command.size()) {
                    char next=command[i + 1];
                    if(next=='"' || next == '\\' || next == '$' || next == '`' ){
                        current+=next;
                        i++;
                    } else {
                        current += c;
                    }
                }
            }
            else {
                current += c;
            }
        }
    }

    if (state != NORMAL) {
        std::cerr << "syntax error: unmatched quote\n";
        return {};
    }

    if (!current.empty())
        args.push_back(current);

    return args;
}

const char* builtins[] = {
    "echo",
    "exit",
    nullptr
};
std::vector<std::string> path_dirs;

void load_path_dirs() {
    char* path=getenv("PATH");
    if(!path)return;

    std::stringstream ss(path);
    std::string dir;
    while(std::getline(ss,dir,':')) {
        path_dirs.push_back(dir);
    }
}

bool is_executable(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return (st.st_mode & S_IXUSR);
}

std::vector<std::string> find_executable_matches(const std::string& prefix) {
    std::vector<std::string> matches;

    for (const auto& dir:path_dirs) {
        DIR* d=opendir(dir.c_str());
        if(!d)continue;

        struct dirent* ent;
        while((ent=readdir(d))!=nullptr) {
            std::string name=ent->d_name;
            if(name.rfind(prefix,0)==0) {
                std::string full=dir+"/"+name;
                if(access(full.c_str(),X_OK)==0) {
                    matches.push_back(name);
                }
            }
        }
        closedir(d);
    }
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    return matches;
}
std::string longest_common_prefix(const std::vector<std::string>& matches) {
    if(matches.empty())return "";

    std::string prefix=matches[0];

    for(size_t i=1;i<matches.size();i++) {
        size_t j=0;
        while (j<prefix.size() &&
               j<matches[i].size() &&
               prefix[j]==matches[i][j]) {
            j++;
        }
        prefix=prefix.substr(0, j);
        if(prefix.empty())break;
    }

    return prefix;
}

static std::string last_completion_prefix;
static int tab_press_count = 0;
char** completer(const char* text,int start,int end) {
    std::string prefix(text);
    rl_completion_append_character = '\0';

    // Reset TAB count if prefix changed
    if (prefix!=last_completion_prefix) {
        tab_press_count=0;
        last_completion_prefix=prefix;
    }

    auto matches = find_executable_matches(prefix);

    // Include builtins
    for (const char* b : builtins) {
        if (!b) break;
        if (std::string(b).rfind(prefix,0)==0) {
            matches.push_back(b);
        }
    }

    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());

    // No matches → bell
    if (matches.empty()) {
        std::cout<<"\x07"<<std::flush;
        return nullptr;
    }

    // One match→complete immediately
    if (matches.size() == 1) {
        std::string result=matches[0] + " ";
        char** out=(char**)malloc(2*sizeof(char*));
        out[0]=strdup(result.c_str());
        out[1]=nullptr;
        tab_press_count=0;
        last_completion_prefix=result;
        return out;
    }

    // MULTIPLE MATCHES → LCP Check
    std::string lcp=longest_common_prefix(matches);

    if (lcp.size()>prefix.size()){
        rl_replace_line(lcp.c_str(), 1);
        rl_point=rl_end;
        tab_press_count=0;
        last_completion_prefix=lcp;
        return nullptr;
    }

    // No LCP progress → bell / list behavior
    tab_press_count++;

    if (tab_press_count==1) {
        std::cout <<"\x07" << std::flush;
        return nullptr;
    }

    // Second TAB → print all matches
    std::cout << "\n";
    for (size_t i = 0; i < matches.size(); i++) {
        std::cout << matches[i];
        if (i + 1 < matches.size())
            std::cout << "  ";
    }
    std::cout << "\n$ " << prefix << std::flush;

    tab_press_count = 0;
    return nullptr;
}

std::vector<std::string> split_pipeline(std::string& command){
  std::vector<std::string>parts;
  std::string current;
  bool insingle=false,indouble=false;

  for(char c:command){
    if(c=='\'' && indouble==false)insingle=!insingle;
    else if(c=='\"' && insingle==false)indouble=!indouble;
    else if(c=='|' && !insingle && !indouble){
      parts.push_back(current);
      current.clear();
      continue;
    }
    current+=c;
  }
  if(!current.empty())parts.push_back(current);

  return parts;

}


void append_history_on_start(){
  char* histfile=getenv("HISTFILE");
  if(histfile==nullptr)return;

  std::string path=histfile;
  
  std::ifstream file(path);
  if(!file.is_open())return;
  std::string line;
  while(std::getline(file,line)){
    if(line.empty())continue;
    add_history(line.c_str());
  }
  last_written_history=history_length;
}
void append_history_at_last(){
  char* histfile=getenv("HISTFILE");
  if(histfile==nullptr)return;
  HIST_ENTRY ** hist=history_list();

  std::string path=histfile;
  
  std::ofstream file(path,std::ios::app);
  if(!file.is_open())return;
  
  for(int i=last_written_history;i<history_length;i++){
    file<<hist[i]->line<<std::endl;
  }
  last_written_history=history_length;
  return;
}
void execute_single_command(const std::string& command,bool from_pipeline){
    std::vector<std::string>args=parse_command(command);
    redirection r=parse_redirection(args);
    if(args.empty())return;

    if(args[0]=="exit"){
      if(!from_pipeline){
        append_history_at_last(); 
        exit(0);}
      return;
    }
    else if(args.size()>0 && args[0]=="echo"){
      apply_redirections(r);
      for(size_t i=1;i<args.size();i++){
        if(i>1)std::cout<<" ";
        std::cout<<args[i];
      }
      std::cout<<std::endl;
      restore_builtin_redirection();
    }
    else if(args.size()>0 && args[0]=="type"){
        if (args.size() < 2) {
            std::cout << "type: missing argument" << std::endl;
            return;
        }
        std::string temp = args[1];

        if(builtin_command.find(temp)!=builtin_command.end()){
          std::cout<<temp<<" is a shell builtin"<<std::endl;
        }
        else {
          char* path_env=std::getenv("PATH"); //reads the PATH environment variable
          bool found=false;

          if(path_env!=nullptr){
            std::stringstream ss(path_env); //convert PATH string into stream to split it easily 
            std::string dir;

            while(std::getline(ss,dir,':')){      //reads from ss , store in dir , split at :
              std::string full_path=dir+"/"+temp;
              if(access(full_path.c_str(),X_OK)==0){        //full_path.c_str() -> convert to char* needed by access() , X_OK checks if file exits=0 o/w -1
                std::cout<<temp<<" is "<<full_path<<std::endl;
                found=true;
                break;
              }
            }
    
          }
          if(found==false){
            std::cout<<temp<<": not found "<<std::endl;
          }
        }
        
    }
    else if(command=="pwd"){
      // std::cout<<std::filesystem::current_path()<<std::endl;  (modern way)
      char cwd[PATH_MAX];

      if(getcwd(cwd,sizeof(cwd))!=nullptr){
        std::cout<<cwd<<std::endl;
      }
      else{
        perror("getcwd");
      }

    }
    else if(args.size()>0 && args[0]=="cd"){
      std::string path;

      if(args.size()==1){
        char* home=getenv("HOME");
        if(home==nullptr)std::cerr<<"cd: HOME: No such file or directory"<<std::endl;
        else path=home;
      }
      else{
        path=args[1];
      }

      if(path=="~"){
        char* home=getenv("HOME");
        if(home==nullptr)std::cerr<<"cd: HOME: No such file or directory"<<std::endl;
        else path=home;
      }
      if(chdir(path.c_str())!=0){      //change the directory of this process
        std::cout<<"cd: "<<path<<": No such file or directory"<<std::endl;
      }


    }
    else if(args[0]=="history"){
      HIST_ENTRY ** hist=history_list();
      if(args.size()==3 && args[1]=="-r"){
        std::ifstream file(args[2]);

        if(!file.is_open()){
          std::cerr<<"history: cannot read file"<<std::endl;
          return;
        }
        std::string line;
        while(std::getline(file,line)){
          if(line.empty())continue;
          add_history(line.c_str());
        }
        return;
      }
      if(args.size()==3 && args[1]=="-w"){
        std::ofstream file(args[2]);
        if(!file.is_open()){
          std::cerr<<"history: cannot write file"<<std::endl;
        }
        if(!hist)return;
        for(int i=0;hist[i]!=nullptr;i++){
          file << hist[i]->line <<std::endl;
        }
        return;
      }
      if(args.size()==3 && args[1]=="-a"){
        std::ofstream file(args[2],std::ios::app);
        if(!file.is_open()){
          std::cerr<<"history: cannot append to file"<<std::endl;
        }
        if(!hist)return;
        for(int i=last_written_history;hist[i]!=nullptr;i++){
          file << hist[i]->line <<std::endl;
        }
        last_written_history=history_length;
        return;
      }
      
      int i=0;
      if(args.size()==2){     // if history n
        int n=std::stoi(args[1]);
        if(n <= history_length){
          i=history_length-n;
        }
      }
      
      if(!hist)return;

      for(i;hist[i]!=nullptr;i++){
        std::cout<<" "<<i+1<<" "<<hist[i]->line<<std::endl;
      }
    }
    else{
      std::stringstream ss(command);
      std::string word;
      // while(ss >> word)args.push_back(word); //reads one word ,space separated  

      std::string program=args[0];
      char* path_env=std::getenv("PATH"); //reads the PATH environment variable
      bool found=false;
      std::string full_path;

      if(path_env!=nullptr){
          std::stringstream ss(path_env); //convert PATH string into stream to split it easily 
          std::string dir;

          while(std::getline(ss,dir,':')){      //reads from ss , store in dir , split at :
            std::string candidate=dir+"/"+program;
            if(access(candidate.c_str(),X_OK)==0){        //full_path.c_str() -> convert to char* needed by access() , X_OK checks if file exits=0 o/w -1
              full_path=candidate;
              found=true;
              break;
            }
          }

      }
      if(found==false){
        std::cout<<program<<": not found "<<std::endl;
        }
      else{
        std::vector<char*>argv;
        for(auto& s:args)argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);  //mark end of arguments

        pid_t pid=fork();
        if(pid==0){
          apply_redirections(r);
          execv(full_path.c_str(),argv.data());
          perror("execv");
          exit(1);
        }
        else{
          waitpid(pid,nullptr,0); //wait for child to finish , prevents shell from exiting early
        }
      }
    }
  
}
void execute_pipeline(const std::vector<std::string>&parts){
  int prev_fd=-1;
  std::vector<pid_t>pids;

  for(size_t i=0;i<parts.size();i++){
    int pipefd[2];
    if(i<parts.size()-1)pipe(pipefd);

    pid_t pid=fork();

    if(pid==0){
      if(prev_fd!=-1){
        dup2(prev_fd,STDIN_FILENO);
        close(prev_fd);
      }
      if(i<parts.size()-1){
        dup2(pipefd[1],STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
      }
      if (i == parts.size() - 1) {
          std::vector<std::string> tmp = parse_command(parts[i]);
          redirection r = parse_redirection(tmp);
          apply_redirections(r);
      }


      execute_single_command(parts[i],true);
      exit(0);
    }
    pids.push_back(pid);
    if(prev_fd!=-1)close(prev_fd);
    if(i<parts.size()-1){
      close(pipefd[1]);
      prev_fd=pipefd[0];
    }
  }
  for(pid_t pid:pids)waitpid(pid,nullptr,0);
}


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  rl_attempted_completion_function = completer;
  rl_completion_append_character = '\0';
  load_path_dirs();
  append_history_on_start();
  
  // TODO: Uncomment the code below to pass the first stage
  while(true){
    // std::cout << "$ ";
    // std::string command;
    // std::getline(std::cin,command);
    // std::string command;
    char* input=readline("$ ");
    if(!input)break;
    std::string command(input);
    free(input);

    if(!command.empty())add_history(command.c_str());

    std::vector<std::string>pipeline=split_pipeline(command);

    if(pipeline.size()>1){
      execute_pipeline(pipeline);
      continue;
    }
    execute_single_command(command, false);
  }
  append_history_at_last();
}
