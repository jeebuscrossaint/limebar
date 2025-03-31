{
  description = "Limebar - A Wayland status bar";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "limebar";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            pkg-config
            wayland-scanner
          ];

          buildInputs = with pkgs; [
            wayland
            wayland-protocols
            wlroots
            cairo
            libxkbcommon
          ];

          preBuild = ''
            # Generate layer shell protocol
            wayland-scanner client-header \
              protocols/wlr-layer-shell-unstable-v1.xml \
              wlr-layer-shell-unstable-v1-client-protocol.h
            wayland-scanner private-code \
              protocols/wlr-layer-shell-unstable-v1.xml \
              wlr-layer-shell-unstable-v1-client-protocol.c

            # Generate xdg-shell protocol
            wayland-scanner client-header \
              ${pkgs.wayland-protocols}/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
              xdg-shell-client-protocol.h
            wayland-scanner private-code \
              ${pkgs.wayland-protocols}/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
              xdg-shell-client-protocol.c
          '';

          buildPhase = ''
              $CC -g -Wall -Wextra \
                $(pkg-config --cflags wayland-client cairo) \
                -I. \
                -o limebar \
                main.c \
                wlr-layer-shell-unstable-v1-client-protocol.c \
                xdg-shell-client-protocol.c \
                $(pkg-config --libs wayland-client cairo) \
                -lwayland-client
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp limebar $out/bin/
          '';
        };

        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            pkg-config
            wayland-scanner
          ];

          buildInputs = with pkgs; [
            wayland
            wayland-protocols
            wlroots
            cairo           # Add this
            libxkbcommon    # Add this
          ];

          shellHook = ''
            echo "Limebar development shell"

            # Generate layer shell protocol
            wayland-scanner client-header \
              protocols/wlr-layer-shell-unstable-v1.xml \
              wlr-layer-shell-unstable-v1-client-protocol.h
            wayland-scanner private-code \
              protocols/wlr-layer-shell-unstable-v1.xml \
              wlr-layer-shell-unstable-v1-client-protocol.c

            # Generate xdg-shell protocol
            wayland-scanner client-header \
              ${pkgs.wayland-protocols}/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
              xdg-shell-client-protocol.h
            wayland-scanner private-code \
              ${pkgs.wayland-protocols}/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
              xdg-shell-client-protocol.c

            # Create build script
            cat > build.sh << 'EOF'
            #!/bin/sh
            set -e

            cc -g -Wall -Wextra \
              $(pkg-config --cflags wayland-client cairo) \
              -I. \
              -o limebar \
              main.c \
              wlr-layer-shell-unstable-v1-client-protocol.c \
              xdg-shell-client-protocol.c \
              $(pkg-config --libs wayland-client cairo) \
              -lwayland-client
            EOF

            chmod +x build.sh
            echo "Development environment ready. Use ./build.sh to compile."
          '';
        };
      }
    );
}
