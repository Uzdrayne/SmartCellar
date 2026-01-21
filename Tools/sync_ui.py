import pathlib
import shutil

# Chemins
SRC_DIR = pathlib.Path("SquareLineStudio/OUTPUT/SquareLine_Project/libraries/ui")  
OUT_DIR = pathlib.Path("lib/ui")  


def sync_ui():
    # 1. Vérification de la source
    if not SRC_DIR.exists():
        print(f"ERREUR : Le dossier source {SRC_DIR} n'existe pas.")
        return

    # 2. Effacer le dossier de sortie s'il existe
    if OUT_DIR.exists():
        print(f"Nettoyage de {OUT_DIR}...")
        try:
            shutil.rmtree(OUT_DIR)
        except Exception as e:
            print(f"Erreur lors de la suppression : {e}")
            return

    # 3. Copier SRC vers OUT
    print(f"Copie de {SRC_DIR} vers {OUT_DIR}...")
    try:
        shutil.copytree(SRC_DIR, OUT_DIR)
        print("Synchronisation réussie !")
    except Exception as e:
        print(f"Erreur lors de la copie : {e}")

if __name__ == "__main__":
    sync_ui()