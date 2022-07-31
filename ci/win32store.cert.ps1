$cert = New-SelfSignedCertificate `
  -Type Custom `
  -Subject "$Env:OSSIA_WIN32_CERT_SUBJECT" `
  -KeyUsage DigitalSignature `
  -FriendlyName "ossia.io" `
  -CertStoreLocation "Cert:\CurrentUser\My" `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")

$password = ConvertTo-SecureString -String $Env:OSSIA_WIN32_CERT_PASSWORD -Force -AsPlainText

Export-PfxCertificate `
  -cert "Cert:\CurrentUser\My\$($cert.Thumbprint)" `
  -FilePath ossia-selfsigned.pfx `
  -Password $password
