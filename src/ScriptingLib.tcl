

# -- Vim bindings for the application --

proc q {} {
  app:quit
}

proc n {} {
  doc:createDefaultEmpty
}

proc wq {} {
  if {[string lenght [doc:filename]] > 0} {
    doc:save [doc:filename]
    app:quit
  } else {
    puts "No document filename specified"
  }
}

proc bn {} { doc:nextBuffer }
proc bnext {} { doc:nextBuffer }
proc bp {} { doc:prevBuffer }
proc bprev {} { doc:prevBuffer }


# -- A Simple Task app plugin --

# Create a new document
proc task:createDocument {} {
  # Create a new empty document
  doc:createEmpty 2 2
  doc:columnWidth 0 7
  doc:columnWidth 1 60

  # Header
  doc:cell A1 "STATUS"
  doc:cell A2 "------"
  doc:cell B1 "DESCRIPTION"
  doc:cell B2 "-----------"

  todo:new
}

# Add a new item
proc task:new {} {
  set row [doc:rowCount]
  doc:addRow $row
  doc:cell [index new 0 $row] "  ☐"
  app:edit [index new 1 $row]
}

proc task:check {} {
  set row [index row [app:cursor]]
  set text [doc:cell [index new 0 $row]]

  if {[string equal $text "  ☐"]} {
    doc:cell [index new 0 $row] "  ☑"
  } else {
    doc:cell [index new 0 $row] "  ☐"
  }

}
