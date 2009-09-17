
#include <iostream>
#include <stdlib.h>

extern "C" { 
  #include "acpp.h"
}

char *file_name = NULL; /* name of the main ArchC file */
char *isa_filename_with_path = NULL;

void parse_archc_description(char **argv) {
  /* Initializes the pre-processor */
  acppInit(1);
  file_name = (char *)malloc(strlen(argv[1]) + strlen(argv[2]) + 1);
  strcpy(file_name, argv[1]);
  strcpy(file_name + strlen(argv[1]), argv[2]);
  
  /* Parse the ARCH file */
  if (!acppLoad(file_name)) {
    fprintf(stderr, "Invalid file: '%s'.\n", file_name);
    exit(1);
  }

  if (acppRun()) {
    fprintf(stderr, "Parser error in ARCH file.\n");
    exit(1);
  }
  acppUnload();

  /* Parse the ISA file */
  isa_filename_with_path = (char *)malloc(strlen(argv[1]) + strlen(isa_filename) + 1);
  strcpy(isa_filename_with_path, argv[1]);
  strcpy(isa_filename_with_path + strlen(argv[1]), isa_filename);
  
  if (!acppLoad(isa_filename_with_path)) {
    
    fprintf(stderr, "Invalid ISA file: '%s'.\n", isa_filename);
    exit(1);
  }
  if (acppRun()) {
    fprintf(stderr, "Parser error in ISA file.\n");
    exit(1);
  }
  acppUnload();

  free(file_name);
  free(isa_filename_with_path);
}

void print_formats() {
  ac_dec_format* pformat;
  ac_dec_field* pfield;
  for (pformat = format_ins_list; pformat != NULL; pformat = pformat->next) {
    std::cout << pformat->name << ", size:" << pformat->size << std::endl;
    for (pfield = pformat->fields; pfield != NULL; pfield = pfield->next) {
      std::cout << "  " << pfield->name 
                << ", size:" << pfield->size
                << ", first_bit:" << pfield->first_bit
                << std::endl;
    }
  }
}

void print_insns() {
  

}

int main(int argc, char **argv) {
  parse_archc_description(argv);
  print_formats();  

  return 0;
}
