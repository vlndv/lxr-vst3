
# LXR — Production VST3 Resource Packaging
# P12b-4b.2
#
# Copies and verifies the five native DSP resources into:
#   1. The generated VST3 bundle
#   2. The installed VST3 bundle
#
# Any failure terminates the script with a nonzero exit code.

param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,

    [Parameter(Mandatory = $true)]
    [string]$InstallDirectory
)

$ErrorActionPreference = "Stop"

try {
    # Resolve the canonical source directory.
    $sourceDirectory = Join-Path $RepositoryRoot "data"

    # Resolve the generated VST3 bundle.
    $buildBundle = Join-Path $BuildDirectory "LXR.vst3"

    # Resolve the installed VST3 bundle.
    $installBundle = Join-Path $InstallDirectory "LXR.vst3"

    # Define the exact native Engine resource contract.
    $assets = [ordered]@{
        "sine_table.bin"   = 8194
        "saw_table.bin"    = 22528
        "tri_table.bin"    = 22528
        "rec_table.bin"    = 22528
        "crash_sample.bin" = 32768
    }

    # Validate the source directory.
    if (!(Test-Path -LiteralPath $sourceDirectory -PathType Container)) {
        throw "Source data directory missing: $sourceDirectory"
    }

    # Validate source assets before copying anything.
    foreach ($asset in $assets.GetEnumerator()) {
        $sourceFile = Join-Path $sourceDirectory $asset.Key

        if (!(Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
            throw "Missing source asset: $sourceFile"
        }

        $actualSize = (Get-Item -LiteralPath $sourceFile).Length

        if ($actualSize -ne $asset.Value) {
            throw "Invalid size for $($asset.Key): $actualSize"
        }
    }

    # Package both generated and installed bundles.
    foreach ($bundle in @($buildBundle, $installBundle)) {

        # Never manufacture an empty bundle.
        if (!(Test-Path -LiteralPath $bundle -PathType Container)) {
            throw "VST3 bundle missing: $bundle"
        }

        # Ensure the bundle contains its native binary.
        $binary = Join-Path $bundle "Contents\x86_64-win\LXR.vst3"

        if (!(Test-Path -LiteralPath $binary -PathType Leaf)) {
            throw "VST3 binary missing: $binary"
        }

        # Create the DSP resource directory.
        $resourceDirectory = Join-Path $bundle "Contents\Resources\LXR"

        New-Item -ItemType Directory -Force `
            -Path $resourceDirectory | Out-Null

        foreach ($asset in $assets.GetEnumerator()) {

            $sourceFile = Join-Path $sourceDirectory $asset.Key
            $targetFile = Join-Path $resourceDirectory $asset.Key

            # Copy the canonical resource.
            Copy-Item -LiteralPath $sourceFile `
                -Destination $targetFile -Force

            # Verify the installed file size.
            $targetSize = (Get-Item -LiteralPath $targetFile).Length

            if ($targetSize -ne $asset.Value) {
                throw "Packaged resource size mismatch: $targetFile"
            }

            # Verify binary identity with SHA-256.
            $sourceHash = (Get-FileHash -LiteralPath $sourceFile -Algorithm SHA256).Hash
            $targetHash = (Get-FileHash -LiteralPath $targetFile -Algorithm SHA256).Hash

            if ($sourceHash -ne $targetHash) {
                throw "Packaged resource hash mismatch: $targetFile"
            }

            Write-Host "PASS: $($asset.Key) -> $bundle"
        }
    }

    Write-Host "LXR resource packaging: PASS"
    exit 0
}
catch {
    Write-Error "LXR resource packaging FAILED: $_"
    exit 1
}
