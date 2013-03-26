#!/usr/bin/perl -w

use strict;

my $BASEDIR    = `pwd`; chomp($BASEDIR);
my $INSTALLDIR = "$BASEDIR/../install";

my @packages;

my %portals4;
$portals4{name}          = "Portals4 SVN Trunk";
$portals4{directory}     = "portals4";
$portals4{get_cmd}       = "svn checkout http://portals4.googlecode.com/svn/trunk/ $portals4{directory}";
$portals4{preconfig_cmd} = "./autogen.sh";
$portals4{config_cmd}    = "./configure --enable-kitten --enable-ppe --disable-transport-ib --disable-transport-udp --disable-shared --with-xpmem=$INSTALLDIR --prefix=$INSTALLDIR";
push(@packages, \%portals4);

my %ompi;
$ompi{name}              = "Open MPI SVN Trunk";
$ompi{directory}         = "ompi";
$ompi{get_cmd}           = "svn co http://svn.open-mpi.org/svn/ompi/trunk $ompi{directory}";
$ompi{preconfig_cmd}     = "./autogen.pl";
$ompi{config_cmd}        = "./configure --with-platform=snl/kitten --with-pmi=$INSTALLDIR --with-portals4=$INSTALLDIR
to portals>";
push(@packages, \%ompi);

# Download and build external packages
for (my $i=0; $i < @packages; $i++) {
    chdir "$BASEDIR";

	# Get the package
    my %pkg = %{$packages[$i]};
    if (! -d "$pkg{directory}") {
        print "[EX] Getting $pkg{name}.\n";
        system ($pkg{get_cmd});
    } else {
		print "[EX] Already got $pkg{name}.\n";
	}

    chdir "$pkg{directory}";

	# Pre-configure the package
	if (! -e "configure") {
	    print "[EX] Running preconfig command for $pkg{name}.\n";
        system ($pkg{preconfig_cmd});
	} else {
	    print "[EX] Already ran preconfig command for $pkg{name}.\n";
	}

	# Configure the package
	if (! -e "Makefile") {
	    print "[EX] Running config command for $pkg{name}.\n";
        system ($pkg{config_cmd});
	} else {
	    print "[EX] Already ran config command for $pkg{name}.\n";
	}
}
