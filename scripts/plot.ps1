param(
    [Parameter(Mandatory = $true)]
    [string]$Spec
)

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
python (Join-Path $scriptDirectory 'plot.py') $Spec
