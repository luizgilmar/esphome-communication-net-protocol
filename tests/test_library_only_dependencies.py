"""Check dependency selection without importing the component's codegen globals."""
import ast
from pathlib import Path

def test_dependency_selection():
    tree = ast.parse(Path("components/communication_net_protocol/__init__.py").read_text(encoding="utf-8"))
    function = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == "AUTO_LOAD")
    namespace = {}
    exec(compile(ast.Module(body=[function], type_ignores=[]), "autoload-test", "exec"), namespace)
    select = namespace["AUTO_LOAD"]
    assert select({"library_only": True}) == []
    assert select({}) == []
    assert {"light", "cover", "binary_sensor"}.issubset(select({"inbound": {"bindings": [{"completion": {"delay": "1s"}}]}}))
    assert {"light", "binary_sensor"}.issubset(select({"state_snapshot": {}}))

if __name__ == "__main__":
    test_dependency_selection()
    print("Library-only and inbound dependency selection PASS")
