Name:       harbour-skrynja
Summary:    Chatklient med Mistral AI til Sailfish OS med websøgning
Version:    0.9.9
Release:    1
Group:      Applications/Internet
License:    GPL-3.0-only
URL:        https://github.com/storefyr/Skrynja
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   qt5-qtmultimedia
Requires:   qt5-qtmultimedia-plugin-audio-pulseaudio
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Multimedia)
BuildRequires:  desktop-file-utils

%description
Skrynja er en elegant klient til Mistral AI, baseret på Conversations-API'et,
integreret perfekt i Sailfish OS' brugerflade. Nyd intelligente samtaler med
websøgning direkte fra din enhed.

Funktioner:
- Mistral AI via Conversations-API'et (/v1/conversations)
- Websøgning (web_search) i samtaler
- API-nøgle gemt krypteret
- Streaming af svar i realtid
- Native Sailfish-grænseflade med Silica-komponenter
- Samtalehistorik

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5 "VERSION=%{version}"

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications             \
   %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
