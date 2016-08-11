# Source: rhysgodfrey/team-city-xunit-meta-runner
# The Commit SHA for corresponding to this release
$commitId = $Env:APPVEYOR_REPO_COMMIT
# The notes to accompany this release, uses the commit message in this case
$releaseNotes = '[Release] eshare packaged (portable)'
# The folder artifacts are built to
$artifactOutputDirectory = $Env:APPVEYOR_BUILD_FOLDER
# The name of the file to attach to this release
$artifact = 'eshare.zip'
# The github username
$gitHubUsername = 'marcodiiga'
# The github repository name
$gitHubRepository = 'eshare'
# The github API key (https://github.com/blog/1509-personal-api-tokens)
$gitHubApiKey = $env:gh_token
# Set to true to mark this as a draft release (not visible to users)
$draft = $FALSE
# Set to true to mark this as a pre-release version
$preRelease = $FALSE
$releaseData = @{
   tag_name = [string]::Format("eshare");
   target_commitish = $commitId;
   name = [string]::Format("eshare");
   body = $releaseNotes;
   draft = $draft;
   prerelease = $preRelease;
 }
 $releaseParams = @{
   Uri = "https://api.github.com/repos/$gitHubUsername/$gitHubRepository/releases";
   Method = 'POST';
   Headers = @{
     Authorization = 'Basic ' + [Convert]::ToBase64String(
     [Text.Encoding]::ASCII.GetBytes($gitHubApiKey + ":x-oauth-basic"));
   }
   ContentType = 'application/json';
   Body = (ConvertTo-Json $releaseData -Compress)
 }
 $result = Invoke-RestMethod @releaseParams 
 $uploadUri = $result | Select -ExpandProperty upload_url
 $uploadUri = $uploadUri -replace '\{\?name,label\}', "?name=$artifact"
 $uploadFile = Join-Path -path $artifactOutputDirectory -childpath $artifact
 $uploadParams = @{
   Uri = $uploadUri;
   Method = 'POST';
   Headers = @{
     Authorization = 'Basic ' + [Convert]::ToBase64String(
     [Text.Encoding]::ASCII.GetBytes($gitHubApiKey + ":x-oauth-basic"));
   }
   ContentType = 'application/zip';
   InFile = $uploadFile
 }
Write-Host $uploadUri
Write-Host $uploadFile
$result = Invoke-RestMethod @uploadParams