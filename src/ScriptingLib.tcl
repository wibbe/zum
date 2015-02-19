

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


# -- A Simple Todo app plugin --

# Create a new document
proc todo:new {} {
  # Create a new empty document
  doc:createEmpty 2 2
  doc:columnWidth 0 7
  doc:columnWidth 1 50

  # Header
  doc:cell 0 0 "STATUS"
  doc:cell 0 1 "------"
  doc:cell 1 0 "DESCRIPTION"
  doc:cell 1 1 "-----------"

  todo:item
}

# Add a new item
proc todo:item {} {
  set row [doc:rowCount]
  doc:addRow $row
  doc:cell 0 $row "☐"
  editor:cursor 1 $row
  editor:edit i
}
