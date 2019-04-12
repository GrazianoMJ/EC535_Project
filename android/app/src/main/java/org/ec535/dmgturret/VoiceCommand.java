package org.ec535.dmgturret;

import java.util.ArrayList;

public class VoiceCommand {
    public enum CommandName {
        FIRE,
        PRIME,
        TILT_UP,
        TILT_DOWN,
        INVALID
    }

    private static class CommandOp {
        private CommandName mCommandName;
        private boolean mIsValid;

        // this op location within the sentence
        private int mWordIndex;
        // this op location within the sentence array
        private int mSentenceIndex;

        public CommandOp() {
            // Null constructor
            mWordIndex = 0;
            mCommandName = CommandName.INVALID;
            mIsValid = false;
        }

        public CommandOp(CommandName commandName, int wordIndex) {
            mWordIndex = wordIndex;
            mCommandName = commandName;
            mIsValid = false;
        }

        public CommandName getCommandName() {
            return mCommandName;
        }

        public int getWordIndex() {
            return mWordIndex;
        }

        public boolean isValid() {
            return mIsValid;
        }

        public boolean shouldHaveAnArgument() {
            return mCommandName == CommandName.TILT_UP
                    || mCommandName == CommandName.TILT_DOWN;
        }


    }
    private static class CommandArgument {
        private int mArgument;
        // this argument's index within the word list
        private int mWordIndex;
        private int mSentenceIndex;
        boolean isValid;

        public CommandArgument() {
            // Null constructor
            mArgument = 0;
            mWordIndex = 0;
            isValid = true;
        }

        public CommandArgument(int argument, int wordIndex) {
            mArgument = argument;
            mWordIndex = wordIndex;
            isValid = false;
        }

        public int getArgument() {
            return mArgument;
        }

        public int getWordIndex() {
            return mWordIndex;
        }

        public boolean isValid() {
            return isValid;
        }
    }

    private CommandOp mCommandOp;
    private CommandArgument mArgument;
    private boolean isValid;

    protected VoiceCommand() {
        // Null voice command
        mArgument = new CommandArgument();
        mCommandOp = new CommandOp();
        isValid = true;
    }

    protected VoiceCommand(CommandOp op, CommandArgument arg) {
        mCommandOp = op;
        mArgument = arg;
        isValid = false;
    }

    public boolean isValid() {
        return isValid;
    }

    public int getArgument() {
        return mArgument.getArgument();
    }

    public CommandName getCommandName() {
        return mCommandOp.getCommandName();
    }

    public static CommandOp getCommandOp(ArrayList<String> stringList) {
        int wordIndex = 0;
        if (stringList.size() <= 0)
            return new CommandOp();
        for (wordIndex = 0; wordIndex < stringList.size(); wordIndex++) {
            if (stringList.get(wordIndex).toLowerCase().contains("fire"))
                return new CommandOp(CommandName.FIRE, wordIndex);
            else if (stringList.get(wordIndex).toLowerCase().contains("prime"))
                return new CommandOp(CommandName.PRIME, wordIndex);
            else if (stringList.get(wordIndex).equalsIgnoreCase("up"))
                return new CommandOp(CommandName.TILT_UP, wordIndex);
            else if (stringList.get(wordIndex).equalsIgnoreCase("down"))
                return new CommandOp(CommandName.TILT_DOWN, wordIndex);
            wordIndex += 1;
        }
        return new CommandOp();
    }

    public static CommandArgument getCommandArgument(
            ArrayList<String> stringList, int beginIndex) {
        int wordIndex = 0;
        int argument = -1;
        // should we care about units?
        if (stringList.size() <= 0)
            return new CommandArgument();
        for (wordIndex = beginIndex; wordIndex < stringList.size(); wordIndex++) {
            try {
                argument = Integer.parseInt(stringList.get(wordIndex));
                break;
            } catch (NumberFormatException ignored) { }
            wordIndex += 1;
        }
        if (argument >= 0)
            return new CommandArgument(argument, wordIndex);
        else
            return new CommandArgument();
    }

    public static VoiceCommand getCommand(ArrayList<String> stringList) {
        CommandOp op = VoiceCommand.getCommandOp(stringList);
        CommandArgument arg = VoiceCommand.getCommandArgument(
                stringList, op.getWordIndex());
        if (op.isValid() && (!op.shouldHaveAnArgument() || arg.isValid())) {
            return new VoiceCommand();
        } else {
            return new VoiceCommand(op, arg);
        }
    }
}