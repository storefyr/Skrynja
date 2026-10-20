# Contributing to Skrynja

Merci de votre intérêt pour contribuer à Skrynja ! 🎉

## Comment contribuer

### Signaler des bugs

Si vous trouvez un bug, ouvrez une [issue](https://github.com/storefyr/Skrynja/issues) en incluant :

- Une description claire du problème
- Les étapes pour reproduire le bug
- Le résultat attendu vs le résultat obtenu
- Votre version de Sailfish OS
- Les logs pertinents (si disponibles)

### Proposer des fonctionnalités

Pour proposer une nouvelle fonctionnalité :

1. Vérifiez que la fonctionnalité n'existe pas déjà
2. Ouvrez une issue avec le tag `enhancement`
3. Décrivez clairement le cas d'usage et les bénéfices
4. Attendez les retours avant de commencer le développement

### Soumettre des Pull Requests

#### Prérequis

- Sailfish SDK installé (ou utilisation des GitHub Actions)
- Connaissance de Qt/QML et C++
- Familiarité avec l'UI Sailfish Silica

#### Processus

1. **Fork** le projet

2. **Clone** votre fork
   ```bash
   git clone https://github.com/votre-username/harbour-skrynja.git
   cd harbour-skrynja
   ```

3. **Créez une branche** pour votre fonctionnalité
   ```bash
   git checkout -b feature/ma-fonctionnalite
   ```

4. **Développez** votre fonctionnalité
   - Respectez le style de code existant
   - Commentez le code complexe
   - Testez sur un appareil ou émulateur Sailfish

5. **Commitez** vos changements
   ```bash
   git commit -m "feat: ajout de [fonctionnalité]"
   ```

   Format des messages de commit (conventionnel) :
   - `feat:` nouvelle fonctionnalité
   - `fix:` correction de bug
   - `docs:` documentation
   - `style:` formatage (pas de changement de code)
   - `refactor:` refactoring
   - `test:` ajout de tests
   - `chore:` tâches de maintenance

6. **Pushez** vers votre fork
   ```bash
   git push origin feature/ma-fonctionnalite
   ```

7. **Ouvrez une Pull Request**
   - Décrivez clairement vos changements
   - Référencez les issues liées
   - Ajoutez des captures d'écran si pertinent

#### Build automatique

Le workflow **pr-build.yml** validera automatiquement votre PR en buildant pour `armv7hl`.

✅ Assurez-vous que le build passe avant de demander une review.

Vous pouvez voir le statut du build dans l'onglet "Checks" de votre PR.

## Standards de code

### C++

- Utilisez Qt 5.6 compatible APIs
- Suivez le style Qt (CamelCase pour classes, camelCase pour méthodes)
- Documentez les classes et méthodes publiques
- Utilisez les smart pointers Qt (`QScopedPointer`, `QSharedPointer`)
- Gérez les erreurs avec des signaux Qt

Exemple :
```cpp
class MyClass : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString myProperty READ myProperty NOTIFY myPropertyChanged)

public:
    explicit MyClass(QObject *parent = nullptr);
    QString myProperty() const;

signals:
    void myPropertyChanged();

private:
    QString m_myProperty;
};
```

### QML

- Utilisez les composants Silica pour l'UI
- Respectez les guidelines Sailfish UI/UX
- Nommez les composants en PascalCase
- Utilisez des IDs descriptifs en camelCase
- Préférez les `Connections` aux slots inline

Exemple :
```qml
import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: myPage

    SilicaFlickable {
        anchors.fill: parent

        Column {
            width: parent.width

            PageHeader {
                title: "Ma Page"
            }

            Label {
                text: "Contenu"
                color: Theme.primaryColor
            }
        }
    }
}
```

## Tests

Pour l'instant, les tests sont manuels :

1. Buildez l'application
   ```bash
   sfdk build
   ```

2. Déployez sur un appareil/émulateur
   ```bash
   sfdk deploy --manual
   ```

3. Testez les fonctionnalités ajoutées/modifiées

4. Vérifiez qu'il n'y a pas de régressions

## Structure du projet

Voir [ARCHITECTURE.md](ARCHITECTURE.md) pour une compréhension détaillée de l'architecture.

```
harbour-skrynja/
├── src/                 # Backend Qt C++
│   ├── mistralapi.*     # API client
│   ├── conversationmodel.*
│   └── settingsmanager.*
├── qml/                 # Frontend Silica
│   ├── pages/          # Pages de l'app
│   └── cover/          # Cover active
├── rpm/                # Packaging RPM
├── translations/       # Fichiers de traduction
└── .github/workflows/  # CI/CD
```

## License

En contribuant, vous acceptez que vos contributions soient sous licence MIT, comme le reste du projet.

## Questions ?

N'hésitez pas à ouvrir une issue ou à contacter [@storefyr](https://github.com/storefyr).

---

Merci pour votre contribution ! 💙
