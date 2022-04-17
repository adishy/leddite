class Context:
    # NOT initializing with screen as the screen *could* be different when context is recalled
    def __init__(self):
        pass
    
    def show(self, screen):
        pass

    def desc(self):
        return "Context"

    def restore(self):
        pass

    def switch(self, next_context):
        pass
