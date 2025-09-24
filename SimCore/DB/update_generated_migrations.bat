powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$out='$(IntDir)GeneratedMigrations.h';" ^
  "$srcDir='$(ProjectDir)DB\migration';" ^
  "$files=Get-ChildItem -Path $srcDir -Filter *.sql | Sort-Object Name;" ^
  "$head='// generated' + [Environment]::NewLine + '#pragma once' + [Environment]::NewLine + '#include <vector>' + [Environment]::NewLine + '#include <string>' + [Environment]::NewLine + 'namespace simcore{namespace db{ static inline const std::vector<std::pair<std::string,std::string>> kEmbeddedMigrations = {';" ^
  "Set-Content -Path $out -Value $head -Encoding UTF8;" ^
  "foreach($f in $files){ $name=$f.Name; $txt=[IO.File]::ReadAllText($f.FullName); Add-Content -Path $out -Value ('  {\"'+$name+'\", R\"SQL('+$txt+')SQL\"},') -Encoding UTF8; }" ^
  "Add-Content -Path $out -Value ('}; }} // ns') -Encoding UTF8;"