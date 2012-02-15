set f [open time r]
  set last_time [read $f]
close $f

set twitterok 0

set current_time [clock seconds]

set delta_time [expr $current_time - $last_time]

if { $delta_time > 90 } {

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

  set consumer_key R1bTLrQ46bIOJHNSMLiA
  set consumer_secret vstrikyuzW6rvdPPxV4wsAGyvoSITaXJXJzLZoY6nPI

  set oauth_token 488703816-TLOVdY3hf2qgfEFfVew2Dig3Pf6HDdS5rWIUywcQ
  set oauth_token_secret xlGLosxkb3pmTd4vKsetMF2I2SgSgXjwLemWXudVc

  set from [encoding convertfrom utf-8 [subst [regsub -all {%(..)%(..)} $from {[binary format H* \1\2]}]]]
  set to [encoding convertto utf-8 [subst [regsub -all {%(..)%(..)} $to {[binary format H* \1\2]}]]]

  catch {oauth::query_api http://api.twitter.com/1/statuses/update.json $consumer_key $consumer_secret POST $oauth_token $oauth_token_secret [list status "$flightnum | $reg | $type | $caller | $from -> $to | @ $alt feet | http://jagernot.com/lhr/ | #aviation" lat $lat long $lon]} err
  catch {oauth::query_api http://api.twitter.com/1/account/update_profile_colors.json $consumer_key $consumer_secret POST $oauth_token $oauth_token_secret [list profile_sidebar_fill_color $icao profile_background_color $icao profile_text_color $icao profile_link_color $icao]} err


  if {1} {
    set f [open log w]
    puts $f "----";
    puts $f "[clock format [clock seconds]]"
    foreach {key value} $err {
      puts "key: $key, value = $value"
    }
    puts $f "----";
    close $f
  }

}
