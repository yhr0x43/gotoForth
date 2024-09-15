{
  description = "flake with devShell for general C development";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let pkgs = nixpkgs.legacyPackages.${system}; in
      with pkgs; {
        packages.default = stdenv.mkDerivation {
          pname = "gotoforth";
          version = "0.0.1";

          src = ./.;

          nativeBuildInputs = [
            gcc
            gnumake
          ];
        };
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            gcc
            gcc.man
            gdb
            gnumake
            valgrind
          ];
        };
      });
}
