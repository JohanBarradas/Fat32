#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <string.h>
//helper function that returns tokens of a string
std::vector<std::string> token_separator(char * token) //separates tokens
{
  std::string str = token;
  //std::cout<< str <<std::endl;
  std::string buf;
  std::stringstream ss(str);
  std::vector<std::string> tokens; // Create vector to hold our words

    while (std::getline(ss,buf,'/'))
    {
      tokens.push_back(buf);
    }
    return tokens;
}
/*                                 COLOR FOR SHELL                                       */
//---------------------------------------------------------------------------------------//
void cyan(){ printf("\033[01;36m");}
void green(){ printf("\033[01;32m");}
void yellow(){ printf("\033[01;33m");}
void reset(){ printf("\033[0m");}
void red(){ printf("\033[01;31m");}
//---------------------------------------------------------------------------------------//
//function checks for whitespace, returns 0 if the strlen == #of whitspace chars on string
//TODO change the input value to string obj
int is_only_whitespace(char * string)
{
  int i;
  int empty_count = 0;
  for(i = 0; i<strlen(string);i++)
  {
    if (isspace(string[i]))
    {
      empty_count++;
    }
  }
  if(strlen(string) == empty_count)
  {
    return 0;
  }
  return 1;
}
/********************************************************************/
void openFileSystemFirst(){
  red();
  printf("Error: File system image must be opened first.\n\n");
  reset();
}
