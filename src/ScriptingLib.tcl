
# -- Navigation command bindnings --

bind "u" undo "Undo the last operation"
bind "U" redo "Redo the last operation"
bind "h" navigateLeft "Move cursor left"
bind "l" navigateRight "Move cursor right"
bind "k" navigateUp "Move cursor up"
bind "j" navigateDown "Move cursor down"
bind "y" yankCurrentCell "Copy the content from the current cell to the yank buffer"
bind "n" findNextMatch "Find the next search match"
bind "N" findPreviousMatch "Find the previous match"

bind "p" {
  set str [getYankBuffer]
  if {[string length $str] > 0} {
    cell [cursor] $str
    navigateDown
  }
} "Paste to the current cell and move to the next row"

bind "P" {
  set str [getYankBuffer]
  if {[string length $str] > 0} {
    cell [cursor] $str
    navigateRight
  }
} "Paste to the current cell and move to the next column"

bind "+" {
  foreach col [selection column] {
    set width [columnWidth $col]
    columnWidth $col [expr $width + 1]
  }
} "Increase width of selected columns"

bind "-" {
  foreach col [selection column] {
    set width [columnWidth $col]
    columnWidth $col [expr $width - 1]
  }
} "Decrease width of selected columns"

bind "ac" {
  addColumn [index column [cursor]]
} "Insert a new column where the cursor is"

bind "ar" {
  addRow [index row [cursor]]
} "Insert a new row where the cursor is"

bind "ap" {
  set text [cell [cursor]]
  append text [getYankBuffer]
  cell [cursor] $text
} "Append the yank-buffer to the current cell"

bind "dw" {
  foreach c [selection all] {
    cell $c ""
  }
} "Clear the selected cells"

# -- Vim bindings for the application --

proc q {} {
  quit
}

proc n {} {
  newDocument
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
  newDocument
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
