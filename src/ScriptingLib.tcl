

# -- Vim bindings for the application --

proc q {} {
  app:quit
}

proc n {} {
  doc_createDefaultEmpty
}

proc wq {} {
  if {[string lenght [doc_filename]] > 0} {
    doc_save [doc_filename]
    app:quit
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
  doc_createEmpty 1 3
  doc_columnWidth 1 100

  # Header
  doc_cell A1 "TASK"
  doc_cell A2 "----"

  task_new
}

# Add a new item
proc task_new {} {
  set row [doc_rowCount]
  doc_addRow $row
  doc_cell [index new 0 $row] "☐ "
  app_edit [index new 0 $row]
}

proc task_check {} {
  set row [index row [app_cursor]]
  set text [doc_cell [index new 0 $row]]
  set pos [string first "☐" $text]
  
  if {[string first "☐" $text]} {
    doc_cell [index new 0 $row] "  ☑"
  } else {
    doc_cell [index new 0 $row] "  ☐"
  }

}
