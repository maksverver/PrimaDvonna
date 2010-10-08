#!/usr/bin/perl
use strict;

my %incl = {};

sub include
{
    my ($fn) = @_;
    return if $incl{$fn};
    $incl{$fn} = 1;

    local(*FP);
    open FP, $fn or die "$! ($fn)";
    my ($lineno, $inv) = (1, 1);
    while(my $line=<FP>)
    {
        if($line =~ /#include "(.+?)"/) {
            include($1);
            $inv = 1;
        } elsif($line =~ /#include <(.+?)>/) {
            if(!$incl{$1}) {
                $incl{$1} = 1;
                print $line;
            }
        } else {
            print "#line $lineno \"$fn\"\n" if $inv;
            $inv = 0;
            print $line;
        }
        ++$lineno;
    }
    close FP;
}


#print "#define NDEBUG\n";
print "#undef EXTERN\n";
print "#define EXTERN static\n";
foreach my $fn (@ARGV) {
    include $fn;
}
