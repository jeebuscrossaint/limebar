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
        # Build configuration
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
            pango
          ];

          # Generate protocols and build in one phase
          buildPhase = ''
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

            # Build the program
            $CC -g -Wall -Wextra \
              $(pkg-config --cflags wayland-client cairo pango pangocairo) \
              -I. \
              -o limebar \
              main.c \
              wlr-layer-shell-unstable-v1-client-protocol.c \
              xdg-shell-client-protocol.c \
              $(pkg-config --libs wayland-client cairo pango pangocairo) \
              -lwayland-client
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp limebar $out/bin/
          '';
        };

        # Development shell
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            pkg-config
            wayland-scanner
            gcc
            gnumake
          ];

          buildInputs = with pkgs; [
            wayland
            wayland-protocols
            wlroots
            cairo
            libxkbcommon
            pango
          ];

          shellHook = ''
            echo "Limebar development shell"

            # Create build script
            cat > build.sh << 'EOF'
            #!/bin/sh
            set -e

            echo "Generating protocols..."

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

            echo "Building limebar..."
            cc -g -Wall -Wextra \
              $(pkg-config --cflags wayland-client cairo pango pangocairo) \
              -I. \
              -o limebar \
              main.c \
              wlr-layer-shell-unstable-v1-client-protocol.c \
              xdg-shell-client-protocol.c \
              $(pkg-config --libs wayland-client cairo pango pangocairo) \
              -lwayland-client

            echo "Build complete!"
            EOF

            chmod +x build.sh
            echo "Development environment ready. Use ./build.sh to compile."
          '';
        };
      }
    );
}
