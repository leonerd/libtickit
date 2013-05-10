#!/usr/bin/perl

use strict;
use warnings;

use Convert::Color;
use Convert::Color::XTerm;

use List::UtilsBy qw( min_by );

print <<EOF;
static struct {
  unsigned int as16 : 4;
  unsigned int as8 : 3;
} xterm256[] = {
EOF

my @XTerm16 = map { Convert::Color::XTerm->new( $_ ) } 0 .. 15;
my @XTerm8  = @XTerm16[0..7];

foreach my $index ( 0 .. 255 ) {
   my $col_rgb = Convert::Color::XTerm->new( $index )->as_rgb;

   if( $index == 232 ) {
      # Once we get to the greys at the end, restrict the lists down to
      # only grey colours

      @XTerm16 = @XTerm16[0,7,8,15];
      @XTerm8  = @XTerm8[0,7];
   }

   my $col16 = min_by { $col_rgb->dst_rgb_cheap( $_->as_rgb ) } @XTerm16;
   my $col8 = min_by { $col_rgb->dst_rgb_cheap( $_->as_rgb ) } @XTerm8;

   printf "  // %d - %d\n  ", $index, $index + 7 if $index % 8 == 0;
   printf "{ %2d, %d }, ", $col16->index, $col8->index;
   print "\n" if $index % 8 == 7;
}

print "};\n";
