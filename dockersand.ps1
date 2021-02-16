# *****************************************************************************
# This file is part of dirtsand.
#
# dirtsand is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# dirtsand is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with dirtsand.  If not, see <http://www.gnu.org/licenses/>.
# *****************************************************************************

param(
    [Parameter(Position=0)]
    [ValidateSet("attach", "build", "help", "restart", "start", "status", "stop")]
    [string]
    $Command = "help"
)

# --- CONFIGURABLE ---
$ComposeFile = "docker-compose.yml"

# The directory this script is in. If you customize this file outside of the repo, hardcode the
# path to the DIRTSAND sources.
$DSDir = $PSScriptRoot

# The prefix for the containers. Defaults to the name of the DIRTSAND source directory ("dirtsand"),
# so the containers are usually: dirtsand_moul_1 and dirtsand_db_1. Change this to something else
# if you intend to run multiple shards on the same machine.
$ProjectName = Split-Path -Leaf $DSDir

function Get-DirtsandContainer() {
    $containers = @(docker-compose ps | Select-String ".*_moul_[0-9]*").Matches
    if ($containers.Length -gt 0) {
        return $containers[0].Value
    }
    return $false
}

function Test-Docker() {
    try {
        Get-Command -ErrorAction Stop docker | Out-Null
        Get-Command -ErrorAction Stop docker-compose | Out-Null
    } catch {
        throw "ERROR: Docker does not seem to be installed. Install docker desktop for Windows from https://www.docker.com/products/docker-desktop"
    }
}

# docker-compose requires the CWD have the docker-compose.yml file.
Push-Location $DSDir
trap { Pop-Location }

if ($Command -eq "help") {
    Write-Host "dockersand - DIRTSAND for Docker"
    Write-Host "Available commands:"
    Write-Host "    attach - attach to the DIRTSAND console"
    Write-Host "    build - rebuild the DIRTSAND containers from the source and compose files"
    Write-Host "    restart - restart the DIRTSAND containers"
    Write-Host "    start - start the DIRTSAND containers"
    Write-Host "    status - check if the DIRTSAND containers are running"
    Write-Host "    stop - stop the DIRTSAND containers"
} elseif ($Command -eq "attach") {
    Test-Docker
    $container = Get-DirtsandContainer
    if (!$container) {
        Write-Warning "dockersand is not running! Starting..."
        docker-compose -p $ProjectName -f $ComposeFile up -d
        $container = Get-DirtsandContainer
    }
    if (!$container) {
        throw "ERROR: Unable to determine DIRTSAND container name. Sorry :("
    }
    Write-Host "Attaching to DIRTSAND... Press Ctrl+P Ctrl+Q to detatch!"
    docker attach $container
} elseif($Command -eq "build") {
    Test-Docker
    $running = Get-DirtsandContainer
    if ($running) {
        Write-Host -ForegroundColor Cyan "Stopping dockersand..."
        docker-compose down
    }
    Write-Host -ForegroundColor Cyan "Building dockersand..."
    docker-compose -p $ProjectName -f $ComposeFile build
    if ($LASTEXITCODE -ne 0) {
        throw "FAILED"
    }
    if ($running) {
        Write-Host -ForegroundColor Cyan "Starting dockersand..."
        docker-compose -p $ProjectName -f $ComposeFile up -d
        if ($LASTEXITCODE -eq 0) {
            Write-Host -NoNewline -ForegroundColor Green "SUCCESS"
            Write-Host ": To manage DIRTSAND, use ./dockersand attach"
        } else {
            throw "FAILED"
        }
    }
} elseif ($Command -eq "restart") {
    Test-Docker
    if (Get-DirtsandContainer) {
        Write-Host -ForegroundColor Cyan "Restarting dockersand..."
        docker-compose restart
    } else {
        Write-Warning "dockersand is not running! Starting..."
        docker-compose -p $ProjectName -f $ComposeFile up -d
    }
    if ($LASTEXITCODE -eq 0) {
        Write-Host -NoNewline -ForegroundColor Green "SUCCESS"
        Write-Host ": To manage DIRTSAND, use ./dockersand attach"
    } else {
        throw "FAILED"
    }
} elseif ($Command -eq "start") {
    Test-Docker
    if (!$(Get-DirtsandContainer)) {
        Write-Host -ForegroundColor Cyan "Starting dockersand..."
        docker-compose -p $ProjectName -f $ComposeFile up -d
        if ($LASTEXITCODE -eq 0) {
            Write-Host -NoNewline -ForegroundColor Green "SUCCESS"
            Write-Host ": To manage DIRTSAND, use ./dockersand attach"
        } else {
            throw "FAILED"
        }
    } else {
        Write-Warning "dockersand is already running!"
    }
} elseif ($Command -eq "status") {
    Test-Docker
    Write-Host -NoNewline "dockersand is "
    if (Get-DirtsandContainer) {
        Write-Host -ForegroundColor Green "UP"
    } else {
        Write-Host -ForegroundColor Red "DOWN"
    }
} elseif ($Command -eq "stop") {
    Test-Docker
    if (Get-DirtsandContainer) {
        Write-Host -ForegroundColor Cyan "Stopping dockersand..."
        docker-compose down
    } else {
        Write-Warning "dockersand is not running!"
    }
} else {
    # This should never happen due to the validate set.
    throw "ERROR: Unrecognized option $Command"
}
