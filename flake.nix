{
  description = "A flake providing a development environment for Limo";

  inputs = {
    flake-parts.url = "github:hercules-ci/flake-parts";
    make-shell.url = "github:nicknovitski/make-shell";
    # Pinned so the build is reproducible AND so we get a libloot new enough
    # to provide loot::GameType::openmw (nixpkgs bumped it to 0.25.5 in this rev;
    # older revs pinned 0.24.5 which lacks it and fails to compile Limo).
    nixpkgs.url = "github:NixOS/nixpkgs/34ab99075ac4f7e40cf037eef32cb1c360bb85e9";
  };

  outputs =
    inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      imports = with inputs; [
        make-shell.flakeModules.default
      ];

      perSystem =
        {
          config,
          pkgs,
          system,
          ...
        }:
        {
          # Required for unrar
          _module.args.pkgs = import inputs.nixpkgs {
            inherit system;
            config.allowUnfree = true;
          };

          packages.default = pkgs.limo.overrideAttrs (final: prev: {
            src = ./.;
            # Our sources require liblz4/libzstd via pkg-config, which the
            # upstream nixpkgs expression does not currently provide.
            nativeBuildInputs = (prev.nativeBuildInputs or [ ]) ++ [
              pkgs.pkg-config
              pkgs.lz4.dev
              pkgs.zstd.dev
              # Catch2 (unit tests, BUILD_TESTING=ON). Going through the cmake
              # nativeBuildInputs hook puts its Catch2Config.cmake on
              # CMAKE_PREFIX_PATH so find_package(Catch2) works in nix develop.
              pkgs.catch2
            ];
            buildInputs = (prev.buildInputs or [ ]) ++ [
              pkgs.lz4
              pkgs.zstd
            ];
            meta.mainProgram = "limo";
          });

          make-shells.default = {
            packages =
              with pkgs;
              [
                clang-tools
                gcc
                gdb
                git
              ]
              ++ config.packages.default.nativeBuildInputs
              ++ config.packages.default.buildInputs;
          };
        };
    };
}
