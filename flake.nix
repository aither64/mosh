{
  description = "Mosh development shell";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          autoconf
          automake
          bash-completion
          gcc
          glibcLocales
          gnumake
          less
          libtool
          libutempter
          ncurses
          openssl
          perl
          pkg-config
          protobuf
          tmux
          zlib
        ];

        shellHook = ''
          export LANG=en_US.UTF-8
          export LC_ALL=en_US.UTF-8
          export LOCALE_ARCHIVE="${pkgs.glibcLocales}/lib/locale/locale-archive"
        '';
      };
    };
}
