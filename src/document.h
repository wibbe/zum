
#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "str.h"

void doc_create_empty();
void doc_close();

int doc_get_column_width(int column);
int doc_get_row_count();
int doc_get_column_count();

void doc_save(String filename);
void doc_load(String filename);

String * doc_get_filename();


String * doc_get_cell_text(int x, int y);
void doc_set_cell_text(int x, int y, String text);

void doc_row_to_label(String dest, int row);
void doc_column_to_label(String dest, int col);


#endif