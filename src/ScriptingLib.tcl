
# -- Vim bindings for the application --

proc q {} {
  quit
}

proc n {} {
  createDefaultEmpty
}

proc e {filename} {
  if {[string length $filename] > 0} {
    load $filename
  } else {
    puts "No filename specified"
  }
}

proc w {args} {
  if {[string length $args] > 0} {
    set filename $args
  } else {
    set filename [filename]
  }

  if {[save $filename]} {
    puts "Document saved!"
  } else {
    puts "Could not save document"
  }
}

proc wq {} {
  if {[string length [filename]] > 0} {
    save [filename]
    quit
  } else {
    puts "No document filename specified"
  }
}

proc bn {} { nextBuffer }
proc bnext {} { nextBuffer }
proc bp {} { prevBuffer }
proc bprev {} { prevBuffer }


# -- A Simple Task app plugin --

# Create a new document
proc task_createDocument {} {
  # Create a new empty document
  createEmpty 3 1
  columnWidth A 5
  columnWidth B 12
  columnWidth C 100

  # Headers
  cell A1 ""
  cell B1 "Date"
  cell C1 "Description"

  task_new Fill the task list
}

# Add a new item
proc task_new args {
  set row [rowCount]
  addRow 0

  cell A2 "\[ \]"
  cell B2 [clock format [clock seconds] -format "%Y-%m-%d"]

  if {[llength $args] > 0} {
    cell C2 [join $args]
  }

  cursor C2
}

proc task_done {} {
  set row [index row [cursor]]
  set text [cell [index new 0 $row]]

  if {[string equal "\[ \]" $text]} {
    cell [index new 0 $row] "\[X\]"
  } else {
    cell [index new 0 $row] "\[ \]"
  }
}

bind " " task_done "Toggle the done status of a task"
