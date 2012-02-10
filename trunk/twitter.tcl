set f [open time r]
  set last_time [read $f]
close $f

set twitterok 0

set current_time [clock seconds]

if { ($current_time - $last_time) > 140 } {
  set twitterok 1
  lappend auto_path /usr/local/lib/tcllib1.14

  source oauth.tcl

  package require json

  set consumer_key qg6FNclfUFj7QgVNlmPr3g
  set consumer_secret qh9TWL80iaxyeq7kjguP759nS1nasEM3iGcAQFbToUk

  set oauth_token 488451779-kSJoLkCD4gsQFrosctFGD7pGZOcyisU1ZEhD8fTF
  set oauth_token_secret Sx4eZru1uVHoOzjiMnKdpH5xCPBg93Ac9amiyskaE

  set f [open time w]
  puts $f $current_time
  close $f

}

