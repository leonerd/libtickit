#!/usr/bin/perl

use strict;
use warnings;

# Copy from RenderBuffer.pm
use constant {
   LINE_SINGLE => 0x01,
   LINE_DOUBLE => 0x02,
   LINE_THICK  => 0x03,
};

# Bitmasks on Cell linemask
use constant {
   # Connections to the next cell upwards
   NORTH        => 0x03,
   NORTH_SINGLE => 0x01,
   NORTH_DOUBLE => 0x02,
   NORTH_THICK  => 0x03,
   NORTH_SHIFT  => 0,

   # Connections to the next cell to the right
   EAST         => 0x0C,
   EAST_SINGLE  => 0x04,
   EAST_DOUBLE  => 0x08,
   EAST_THICK   => 0x0C,
   EAST_SHIFT   => 2,

   # Connections to the next cell downwards
   SOUTH        => 0x30,
   SOUTH_SINGLE => 0x10,
   SOUTH_DOUBLE => 0x20,
   SOUTH_THICK  => 0x30,
   SOUTH_SHIFT  => 4,

   # Connections to the next cell to the left
   WEST         => 0xC0,
   WEST_SINGLE  => 0x40,
   WEST_DOUBLE  => 0x80,
   WEST_THICK   => 0xC0,
   WEST_SHIFT   => 6,
};
my @linechars;

while( <DATA> ) {
   chomp;
   my ( $char, $spec ) = split( m/\s+=>\s+/, $_, 2 );

   my $mask = 0;
   $mask |= __PACKAGE__->$_ for $spec =~ m/([A-Z_]+)/g;

   $linechars[$mask] = ord $char;
}

close DATA;

# Fill in the gaps
foreach my $mask ( 1 .. 255 ) {
   next if defined $linechars[$mask];

   # Try with SINGLE instead of THICK, so mask away 0xAA
   if( my $char = $linechars[$mask & 0xAA] ) {
      $linechars[$mask] = $char;
      next;
   }

   # The only ones left now are awkward mixes of single/double
   # Turn DOUBLE into SINGLE
   my $singlemask = $mask;
   foreach my $dir (qw( NORTH EAST SOUTH WEST )) {
      my $dirmask = __PACKAGE__->$dir;
      my $dirshift = __PACKAGE__->${\"${dir}_SHIFT"};

      my $dirsingle = LINE_SINGLE << $dirshift;
      my $dirdouble = LINE_DOUBLE << $dirshift;

      $singlemask = ( $singlemask & ~$dirmask ) | $dirsingle
         if ( $singlemask & $dirmask ) == $dirdouble;
   }

   if( my $char = $linechars[$singlemask] ) {
      $linechars[$mask] = $char;
      next;
   }

   die sprintf "TODO: Couldn't find a linechar for %02x\n", $mask;
}

# Now output the mask

open STDOUT, ">", ( $0 =~ m/^(.*)\.PL$/ )[0]
   or die "Cannot write $1 - $!\n";

print <<'EOF';
static uint32_t linemask_to_char[] = {
EOF

foreach ( 0 .. $#linechars ) {
   printf "  // 0x%02x\n  ", $_ if $_%16 == 0;
   printf "0x%04x, ", $linechars[$_] // 0;
   printf "\n" if $_%16 == 15;
}

print <<'EOF';
};
EOF

use utf8;
__DATA__
─ => WEST_SINGLE | EAST_SINGLE
━ => WEST_THICK | EAST_THICK
│ => NORTH_SINGLE | SOUTH_SINGLE
┃ => NORTH_THICK | SOUTH_THICK
┌ => SOUTH_SINGLE | EAST_SINGLE
┍ => SOUTH_SINGLE | EAST_THICK
┎ => SOUTH_THICK | EAST_SINGLE
┏ => SOUTH_THICK | EAST_THICK
┐ => SOUTH_SINGLE | WEST_SINGLE
┑ => SOUTH_SINGLE | WEST_THICK
┒ => SOUTH_THICK | WEST_SINGLE
┓ => SOUTH_THICK | WEST_THICK
└ => NORTH_SINGLE | EAST_SINGLE
┕ => NORTH_SINGLE | EAST_THICK
┖ => NORTH_THICK | EAST_SINGLE
┗ => NORTH_THICK | EAST_THICK
┘ => NORTH_SINGLE | WEST_SINGLE
┙ => NORTH_SINGLE | WEST_THICK
┚ => NORTH_THICK | WEST_SINGLE
┛ => NORTH_THICK | WEST_THICK
├ => NORTH_SINGLE | EAST_SINGLE | SOUTH_SINGLE
┝ => NORTH_SINGLE | SOUTH_SINGLE | EAST_THICK
┞ => NORTH_THICK | EAST_SINGLE | SOUTH_SINGLE
┟ => NORTH_SINGLE | EAST_SINGLE | SOUTH_THICK
┠ => NORTH_THICK | EAST_SINGLE | SOUTH_THICK
┡ => NORTH_THICK | EAST_THICK | SOUTH_SINGLE
┢ => NORTH_SINGLE | EAST_THICK | SOUTH_THICK
┣ => NORTH_THICK | EAST_THICK | SOUTH_THICK
┤ => NORTH_SINGLE | WEST_SINGLE | SOUTH_SINGLE
┥ => NORTH_SINGLE | SOUTH_SINGLE | WEST_THICK
┦ => WEST_SINGLE | NORTH_THICK | SOUTH_SINGLE
┧ => NORTH_SINGLE | WEST_SINGLE | SOUTH_THICK
┨ => WEST_SINGLE | NORTH_THICK | SOUTH_THICK
┩ => WEST_THICK | NORTH_THICK | SOUTH_SINGLE
┪ => WEST_THICK | NORTH_SINGLE | SOUTH_THICK
┫ => WEST_THICK | NORTH_THICK | SOUTH_THICK
┬ => WEST_SINGLE | SOUTH_SINGLE | EAST_SINGLE
┭ => WEST_THICK | SOUTH_SINGLE | EAST_SINGLE
┮ => WEST_SINGLE | SOUTH_SINGLE | EAST_THICK
┯ => WEST_THICK | SOUTH_SINGLE | EAST_THICK
┰ => WEST_SINGLE | SOUTH_THICK | EAST_SINGLE
┱ => WEST_THICK | SOUTH_THICK | EAST_SINGLE
┲ => WEST_SINGLE | SOUTH_THICK | EAST_THICK
┳ => WEST_THICK | SOUTH_THICK | EAST_THICK
┴ => WEST_SINGLE | NORTH_SINGLE | EAST_SINGLE
┵ => WEST_THICK | NORTH_SINGLE | EAST_SINGLE
┶ => WEST_SINGLE | NORTH_SINGLE | EAST_THICK
┷ => WEST_THICK | NORTH_SINGLE | EAST_THICK
┸ => WEST_SINGLE | NORTH_THICK | EAST_SINGLE
┹ => WEST_THICK | NORTH_THICK | EAST_SINGLE
┺ => WEST_SINGLE | NORTH_THICK | EAST_THICK
┻ => WEST_THICK | NORTH_THICK | EAST_THICK
┼ => WEST_SINGLE | NORTH_SINGLE | EAST_SINGLE | SOUTH_SINGLE
┽ => WEST_THICK | NORTH_SINGLE | EAST_SINGLE | SOUTH_SINGLE
┾ => WEST_SINGLE | NORTH_SINGLE | EAST_THICK | SOUTH_SINGLE
┿ => WEST_THICK | NORTH_SINGLE | EAST_THICK | SOUTH_SINGLE
╀ => WEST_SINGLE | NORTH_THICK | EAST_SINGLE | SOUTH_SINGLE
╁ => WEST_SINGLE | NORTH_SINGLE | EAST_SINGLE | SOUTH_THICK
╂ => WEST_SINGLE | NORTH_THICK | EAST_SINGLE | SOUTH_THICK
╃ => WEST_THICK | NORTH_THICK | EAST_SINGLE | SOUTH_SINGLE
╄ => WEST_SINGLE | NORTH_THICK | EAST_THICK | SOUTH_SINGLE
╅ => WEST_THICK | NORTH_SINGLE | EAST_SINGLE | SOUTH_THICK
╆ => WEST_SINGLE | NORTH_SINGLE | EAST_THICK | SOUTH_THICK
╇ => WEST_THICK | NORTH_THICK | EAST_THICK | SOUTH_SINGLE
╈ => WEST_THICK | NORTH_SINGLE | EAST_THICK | SOUTH_THICK
╉ => WEST_THICK | NORTH_THICK | EAST_SINGLE | SOUTH_THICK
╊ => WEST_SINGLE | NORTH_THICK | EAST_THICK | SOUTH_THICK
╋ => WEST_THICK | NORTH_THICK | EAST_THICK | SOUTH_THICK
═ => WEST_DOUBLE | EAST_DOUBLE
║ => NORTH_DOUBLE | SOUTH_DOUBLE
╒ => EAST_DOUBLE | SOUTH_SINGLE
╓ => EAST_SINGLE | SOUTH_DOUBLE
╔ => SOUTH_DOUBLE | EAST_DOUBLE
╕ => WEST_DOUBLE | SOUTH_SINGLE
╖ => WEST_SINGLE | SOUTH_DOUBLE
╗ => WEST_DOUBLE | SOUTH_DOUBLE
╘ => NORTH_SINGLE | EAST_DOUBLE
╙ => NORTH_DOUBLE | EAST_SINGLE
╚ => NORTH_DOUBLE | EAST_DOUBLE
╛ => WEST_DOUBLE | NORTH_SINGLE
╜ => WEST_SINGLE | NORTH_DOUBLE
╝ => WEST_DOUBLE | NORTH_DOUBLE
╞ => NORTH_SINGLE | EAST_DOUBLE | SOUTH_SINGLE
╟ => NORTH_DOUBLE | EAST_SINGLE | SOUTH_DOUBLE
╠ => NORTH_DOUBLE | EAST_DOUBLE | SOUTH_DOUBLE
╡ => WEST_DOUBLE | NORTH_SINGLE | SOUTH_SINGLE
╢ => WEST_SINGLE | NORTH_DOUBLE | SOUTH_DOUBLE
╣ => WEST_DOUBLE | NORTH_DOUBLE | SOUTH_DOUBLE
╤ => WEST_DOUBLE | SOUTH_SINGLE | EAST_DOUBLE
╥ => WEST_SINGLE | SOUTH_DOUBLE | EAST_SINGLE
╦ => WEST_DOUBLE | SOUTH_DOUBLE | EAST_DOUBLE
╧ => WEST_DOUBLE | NORTH_SINGLE | EAST_DOUBLE
╨ => WEST_SINGLE | NORTH_DOUBLE | EAST_SINGLE
╩ => WEST_DOUBLE | NORTH_DOUBLE | EAST_DOUBLE
╪ => WEST_DOUBLE | NORTH_SINGLE | EAST_DOUBLE | SOUTH_SINGLE
╫ => WEST_SINGLE | NORTH_DOUBLE | EAST_SINGLE | SOUTH_DOUBLE
╬ => WEST_DOUBLE | NORTH_DOUBLE | EAST_DOUBLE | SOUTH_DOUBLE
╴ => WEST_SINGLE
╵ => NORTH_SINGLE
╶ => EAST_SINGLE
╷ => SOUTH_SINGLE
╸ => WEST_THICK
╹ => NORTH_THICK
╺ => EAST_THICK
╻ => SOUTH_THICK
╼ => WEST_SINGLE | EAST_THICK
╽ => NORTH_SINGLE | SOUTH_THICK
╾ => WEST_THICK | EAST_SINGLE
╿ => NORTH_THICK | SOUTH_SINGLE
