#!/bin/csh -f
# LastEdit: 08aug2014

#  (c) Copyright 2012,2013,2014 Ralf Brown/Carnegie Mellon University	
#      This program is free software; you can redistribute it and/or
#      modify it under the terms of the GNU General Public License as
#      published by the Free Software Foundation, version 3.
#
#      This program is distributed in the hope that it will be
#      useful, but WITHOUT ANY WARRANTY; without even the implied
#      warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#      PURPOSE.  See the GNU General Public License for more details.
#
#      You should have received a copy of the GNU General Public
#      License (file COPYING) along with this program.  If not, see
#      http://www.gnu.org/licenses/

set def_min=25
set def_max=65
set def_minline=100
set min=$def_min
set max=$def_max
set minline=$def_minline
set cpus=4
set testtype="test"

set p="$0:h"
if ( "$p" == "$0" ) set p=.

while ("x$1" =~ x-* )
   switch ($1)
      case '--min':
         shift
	 set min="$1"
	 breaksw
      case '--max':
         shift
	 set max="$1"
	 breaksw
      case '--lines':
         shift
	 set minline="$1"
	 breaksw
      case '-j':
         shift
         set cpus="$1"
	 breaksw
      case '--devtest':
	 set testtype="devtest"
	 breaksw
      case '--tiny':
         set min=20
	 set max=40
	 breaksw
      case '--short':
         set min=$def_min
	 set max=$def_max
	 set minline=$def_minline
	 breaksw
      case '--medium':
         set min="80"
	 set max="120"
	 breaksw
      case '--long':
         set min="120"
	 set max="200"
	 breaksw
      case '--help':
         echo "Usage: $0:t [options]"
	 echo "Options:"
	 echo "  -j N      run N threads in parallel"
	 echo "  --min N   set minimum line length to N bytes"
	 echo "  --max N   set maximum line length to N characters"
	 echo "  --tiny    shorthand for --min 20 --max 40"
	 echo "  --short   shorthand for --min $def_min --max $def_max (default)"
	 echo "  --medium  shorthand for --min 80 --max 120"
	 echo "  --long    shorthand for --min 120 --max 200"
	 echo "  --devtest use *-devtest files instead of *-test"
	 exit 0
	 breaksw
      default:
         echo "Unknown option '$1'.  Use $0:t --help"
	 exit 1
	 breaksw
   endsw
   shift
end

ls eval-data/*.*-${testtype}.* eval-data/*/*.*-${testtype}.* | \
    grep -v -e '~$' -e '[.]unused$' -e '[.]bak$' -e '/unused/' | \
    xargs -n1 '-d\n' -P$cpus csh -f "$p/mktestset.sh" -l auto -m "$minline" "$min" "$max"

# some overrides of the above automatic language selection
csh -f "$p/mktestset.sh" -q -l ipk -m "$minline" "$min" "$max" eval-data/ScriptureEarth/esk_US.Inupiatun-NW-Alaska-test.utf8

## character-set overrides
(setenv LC_ALL pt_PT.iso-8859-1 ; csh -f "$p/mktestset.sh" -q -l auto -m "$minline" $min $max eval-data/pt_PT.Dwelle-test.latin1.txt )
(setenv LC_ALL pt_PT.iso-8859-1 ; csh -f "$p/mktestset.sh" -q -l auto -m "$minline" $min $max eval-data/Misc/jiv_EC.Shuar-test.latin1.txt )
(setenv LC_ALL ro_RO.iso-8859-2 ; csh -f "$p/mktestset.sh" -q -l auto -m "$minline" $min $max eval-data/ro_RO.Romanian-DWelle-test.latin2.txt )

exit 0
