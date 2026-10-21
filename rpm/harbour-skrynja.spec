Name:       harbour-skrynja
Summary:    Client de chat IA pour Sailfish OS avec recherche web
Version:    0.4.5
Release:    1
Group:      Applications/Internet
License:    MIT
URL:        https://github.com/storefyr/Skrynja
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  desktop-file-utils

%description
Skrynja est un client élégant pour Mistral AI, basé sur l'API Conversations,
intégré parfaitement dans l'interface utilisateur de Sailfish OS. Profitez de
conversations intelligentes avec recherche web directement depuis votre appareil.

Fonctionnalités:
- Mistral AI via l'API Conversations (/v1/conversations)
- Recherche web (web_search) dans les conversations
- Clé API stockée brouillée
- Streaming en temps réel des réponses
- Interface native Sailfish avec composants Silica
- Historique des conversations

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
