

# -- Vim bindings for the application --

proc q {} {
  app_quit
}

proc n {} {
  doc_createDefaultEmpty
}

proc e {filename} {
  if {[string length $filename] > 0} {
    doc_load $filename
  } else {
    puts "No filename specified"
  }
}

proc wq {} {
  if {[string lenght [doc_filename]] > 0} {
    doc_save [doc_filename]
    app_quit
  } else {
    puts "No document filename specified"
  }
}

proc bn {} { doc_nextBuffer }
proc bnext {} { doc_nextBuffer }
proc bp {} { doc_prevBuffer }
proc bprev {} { doc_prevBuffer }


# -- A Simple Task app plugin --

# Create a new document
proc task_createDocument {} {
  # Create a new empty document
  doc_createEmpty 3 1
  doc_columnWidth 0 5
  doc_columnWidth 1 12
  doc_columnWidth 2 100

  # Header
  doc_cell A1 "DONE"
  doc_cell B1 "DATE"
  doc_cell C1 "DESCRIPTION"

  task_new Fill the task list
}

# Add a new item
proc task_new args {
  set row [doc_rowCount]
  doc_addRow 0

  doc_cell A2 "\[ \]"
  doc_cell B2 [clock format [clock seconds] -format "%Y-%m-%d"]

  if {[llength $args] > 0} {
    doc_cell C2 [join $args]
  }

  app_cursor C2
}

proc task_done {} {
  set row [index row [app_cursor]]
  set text [doc_cell [index new 0 $row]]

  if {[string equal "\[ \]" $text]} {
    doc_cell [index new 0 $row] "\[X\]"
  } else {
    doc_cell [index new 0 $row] "\[ \]"
  }
}
