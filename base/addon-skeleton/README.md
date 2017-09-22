## A skeleton of process add-on for score.

## Usage

* Rename skeleton by whatever name you wish: 

Linux:

    # perl-rename on Arch Linux & derivatives
    # rename on Debian, Ubuntu, Mint & derivatives
    shopt -s globstar # on bash only
    perl-rename 's/skeleton/mystuff/' **/*.{hpp,cpp}
    sed -i 's/skeleton/mystuff/g' **/*.{hpp,cpp}

Mac:

    brew install rename gnu-sed
    shopt -s globstar # on bash only
    rename 's/skeleton/mystuff/' **/*
    gsed -i 's/skeleton/mystuff/g' **/*

* Generate new uuids

    perl -pi -e 'chomp(my $uidgen = `uuidgen`);s|00000000-0000-0000-0000-000000000000|$uidgen|gi' **/*.{hpp,cpp}
