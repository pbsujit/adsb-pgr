set f [open time r]
  set last_time [read $f]
close $f

set twitterok 0

set current_time [clock seconds]

set delta_time [expr $current_time - $last_time]

if { $delta_time > 20 } {

  set twitterok 1

  set f [open time w]
    puts $f $current_time
  close $f

} else {
  puts "$delta_time is delta"
}

if {$twitterok} {

  puts "posting twitter";

  lappend auto_path /usr/local/lib/tcllib1.14
  source oauth.tcl
  package require json

  set consumer_key qg6FNclfUFj7QgVNlmPr3g
  set consumer_secret qh9TWL80iaxyeq7kjguP759nS1nasEM3iGcAQFbToUk

  set oauth_token 488703816-NQTamyQdrxvehyQIkNnK5NuCdKT7URlqn9xXmu6m
  set oauth_token_secret BYERXmLerBjRUvDCVHz1WS1ICbyJ39ehmvpYPyCWZKY

  catch {oauth::query_api http://api.twitter.com/1/statuses/update.json $consumer_key $consumer_secret POST $oauth_token $oauth_token_secret [list status "$flightnum | $reg | $caller | $from -> $to | @ $alt feet"]} err

  set f [open log w]
  puts $f "----";
  puts $f "[clock format [clock seconds]]"
  foreach {key value} $err {
    puts "key: $key, value = $value"
  }
  puts $f "----";
  close $f

}
