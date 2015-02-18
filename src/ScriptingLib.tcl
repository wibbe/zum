
# -- Create a mapping from Zum commands to vim like commands --

debug 1

proc q {} {
  app:quit
}

proc wq {} {
  if {[string lenght "[doc:filename]"] > 0} {
    doc:save [doc:filename]
    app:quit
  } else {
    puts "No document filename specified"
  }
}
