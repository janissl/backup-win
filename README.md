# backup-win

A simple Unicode-aware Windows commandline utility to copy latest versions of files to a user-defined location.<br><br>
* A structure of source directories is preserved at the destination.<br>
* The path length must not exceed the MAX_PATH value.<br>
* If the destination root does not exist, its parent directory must be existing.<br>
* A log file (named _last.log_) is saved next to the executable file on each run and contains all source and destination
paths as well as error messages if some errors have occurred.<br>
#### Usage:
`.\backup-win.exe <source_root_directory> <destination_root_directory>`
