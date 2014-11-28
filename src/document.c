
#include "document.h"
#include "str.h"

#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_ROW_COUNT 10
#define DEFAULT_COL_COUNT 10
#define DEFAULT_COL_WIDTH 12
#define CELL_DATA_LEN 256

typedef struct {
  String data;
} Cell;

static int doc_width;
static int doc_height;

static int * doc_column_width;
static Cell ** doc_cells;
static String doc_filename;

int doc_cell_count()
{
  return doc_width * doc_height;
}

void doc_create_empty()
{
  doc_width = DEFAULT_ROW_COUNT;
  doc_height = DEFAULT_COL_COUNT;
  doc_column_width = malloc(sizeof(int) * doc_width);
  doc_cells = malloc(sizeof(struct Cell *) * doc_cell_count());
  memset(doc_cells, 0, sizeof(struct Cell *) * doc_cell_count());
  string_set(doc_filename, "[NoName]");

  for (int i = 0; i < doc_width; ++i)
    doc_column_width[i] = DEFAULT_COL_WIDTH;
}

void doc_close()
{
  free(doc_column_width);

  for (int i = 0; i < doc_cell_count(); ++i)
  {
    if (doc_cells[i])
      free(doc_cells[i]);
  }

  free(doc_cells);
}

String * doc_get_filename()
{
  return &doc_filename;
}

static void write_row(FILE * file, int row)
{
  char cell[STRING_LEN];

  for (int column = 0; column < doc_width; ++column)
  {
    string_utf8(cell, *doc_get_cell_text(column, row));

    fprintf(file, "%s", cell);
    if (column < (doc_width - 1))
      fprintf(file, ",");
  }
  fprintf(file, "\n");
}

void doc_save(String filename)
{
  char filename_utf8[STRING_LEN];
  string_utf8(filename_utf8, filename);

  FILE * file = fopen(filename_utf8, "w");
  if (!file)
    return;

  // Write header info
  for (int i = 0; i < doc_width; ++i)
  {
    fprintf(file, "%d", doc_column_width[i]);
    if (i < (doc_width - 1))
      fprintf(file, ",");
  }
  fprintf(file, "\n");

  for (int row = 0; row < doc_height; ++row)
    write_row(file, row);

  fclose(file);

  string_copy(doc_filename, filename);
}

static char * read_cell(char * it, String cell)
{
  char * start = it;

  while (*it != ',' && *it != '\0' && *it != '\n')
    ++it;

  char curr = *it;
  *it = '\0';

  string_set(cell, start);
  *it = curr;
  return it;
}

static void read_column_sizes(char * data)
{
  String cell;
  char * it = data;

  while (1)
  {
    it = read_cell(it, cell);
    if (*it == '\n' || *it == '\0')
      return;


  }
}

int doc_load(String filename)
{
  char filename_utf8[STRING_LEN];
  string_utf8(filename_utf8, filename);

  FILE * file = fopen(filename_utf8, "w");
  if (!file)
    return 0;

  fseek(file, 0, SEEK_END);
  const long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char * data = malloc(size);
  fread(data, size, 1, file);
  fclose(file);


  free(data);
  return 0;
}

int doc_get_column_width(int column)
{
  assert(column < doc_width);
  return doc_column_width[column];
}

int doc_get_row_count()
{
  return doc_width;
}

int doc_get_column_count()
{
  return doc_height;
}

static int get_cell_index(int x, int y)
{
  return (y * doc_width) + x;
}

static Cell * get_cell(int x, int y)
{
  return doc_cells[get_cell_index(x, y)];
}

String * doc_get_cell_text(int x, int y)
{
  assert(x < doc_width);
  assert(y < doc_height);

  Cell * cell = get_cell(x, y);
  if (cell)
    return &cell->data;
  return &EMPTY_STR;
}

void doc_set_cell_text(int x, int y, String text)
{
  assert(x < doc_width);
  assert(y < doc_height);

  Cell * cell = get_cell(x, y);
  if (!cell)
  {
    cell = (Cell *)malloc(sizeof(Cell));
    doc_cells[get_cell_index(x, y)] = cell;
  }

  string_copy(cell->data, text);
}

void doc_row_to_label(String dest, int row)
{
  string_fmt(dest, "%5d ", row + 1);
}

void doc_column_to_label(String dest, int col)
{

}


