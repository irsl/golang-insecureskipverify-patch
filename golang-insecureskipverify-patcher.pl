#!/usr/bin/perl

use strict;
use warnings;
use Fcntl qw(SEEK_SET SEEK_CUR SEEK_END); # better than using 0, 1, 2

my %patches = (
   "go116-amd64" => { "before" => "0f855a060000", "after" => "e95b06000090" },
   "go113-amd64" => { "before" => "0f8532050000", "after" => "e93305000090" },
);

my $in = shift @ARGV;
my $mode = shift @ARGV;
my $modestr = join(" ", keys %patches);
die "Usage: $0 path-to-go-binary-to-patch mode
Where mode is one of: $modestr
" if ((!$in)||(!$mode)||(!$patches{$mode}));

my $patch = $patches{$mode};

open(my $fh, "+<", $in) or die "cannot open $in: $!";
binmode $fh;
my $filesize = -s $in;
read($fh, my $buf, $filesize);

my $bufsiz = length($buf);
die "Unable to read the full binary ($bufsiz != $filesize)" if($bufsiz != $filesize);



my $regex_pattern = "";
while($patch->{'before'} =~ /(..)/g) {
	$regex_pattern .= "\\x$1";
}

printf "Looking for: %s\n", $patch->{before};
my @ret;
while($buf =~ m#$regex_pattern#g) {
	push @ret, [ $-[0], $+[0] ];
}
my $hits = scalar @ret;
die "Pattern not found :(" if(!$hits);

if($hits > 1) {
  die "There are multiple hits..." if(!$ENV{FORCE});
}

print "$hits match!\n";

printf "Replacing to: %s\n", $patch->{after};
my $first_hit = $ret[0];
printf "%08x\n", $first_hit->[0];
seek($fh, $first_hit->[0], SEEK_SET);
print $fh pack("H*", $patch->{after});
close($fh);
print "ready, file patched in place\n";
