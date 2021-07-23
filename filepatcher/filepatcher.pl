#!/usr/bin/perl

use strict;
use warnings;
use Fcntl qw(SEEK_SET SEEK_CUR SEEK_END); # better than using 0, 1, 2

my $in = shift @ARGV;
my $patch_from = shift @ARGV;
my $patch_to = shift @ARGV;

die "Usage: $0 path-to-go-binary-to-patch patch-from patch-to
" if ((!$in)||(!$patch_to));

my $patch = {"before"=>$patch_from, "after"=>$patch_to};

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
