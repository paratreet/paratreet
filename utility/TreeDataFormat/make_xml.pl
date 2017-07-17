#!/usr/bin/perl -w

$outcfg = "description.xml";
open(CFG,">$outcfg") or die "Couldn't write to $outcfg";

print CFG <<"DONE";
<?xml version="1.0" encoding="iso-8859-1"?>
<simulation>
DONE
opendir THISDIR, "." or die "Couldn't open current directory: $!";
@dirs = grep /.*[^\.][^x].*/, readdir THISDIR;

foreach $dir (@dirs) {
next if ($dir =~ /description/);
print CFG "\t<family name=\"$dir\">\n";
print "$dir\n";
opendir LOWERDIR, $dir or die "Couldn't open current directory: $!";
@files = grep /[^\.]/, readdir LOWERDIR;
foreach $file (@files) {
if ($file =~ /pos/){$attrib="position";}
elsif ($file =~ /pot/){$attrib="potential";}
elsif ($file =~ /vel/){$attrib="velocity";}
else {$attrib = $file;}
print CFG "\t\t<attribute name=\"$attrib\" link=\"$dir/$file\"/>\n";
}
close(LOWERDIR);
print CFG "\t</family>\n";
}
print CFG "</simulation>\n";
close(THISDIR);
close(CFG);
